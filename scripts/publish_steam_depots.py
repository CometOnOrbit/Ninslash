#!/usr/bin/env python3
"""Build, stage, verify and optionally upload all Ninslash Steam depots."""

import argparse
import getpass
import os
import pty
import re
import select
import shutil
import subprocess
import sys
import termios
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TRANSIENT_STEAMPIPE_HTTP = re.compile(r"\bHTTP\s+(5\d\d)\b", re.IGNORECASE)
ANSI_ESCAPE = re.compile(r"\x1b(?:\[[0-?]*[ -/]*[@-~]|\][^\x07]*(?:\x07|\x1b\\))")
PASSWORD_PROMPT = re.compile(r"\b(?:password|passphrase)\s*:", re.IGNORECASE)
STEAM_GUARD_PROMPT = re.compile(
    r"(?:steam\s*guard|two[- ]factor|authenticator)[^\r\n:]*(?:code|token)[^\r\n:]*:",
    re.IGNORECASE,
)
LOGIN_PROMPT_TIMEOUT = 60.0


class SteamUploadError(RuntimeError):
    def __init__(self, returncode, attempts, transient_http_statuses=(), output="", job_label=""):
        label = f" ({job_label})" if job_label else ""
        super().__init__(f"SteamCMD exited with {returncode}{label}")
        self.returncode = returncode
        self.attempts = attempts
        self.transient_http_statuses = tuple(sorted(set(transient_http_statuses)))
        self.output = output or ""
        self.job_label = job_label or ""


class SteamInteractionError(RuntimeError):
    pass


def run(command, cwd=ROOT):
    print("+", " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], cwd=cwd, check=True)


def run_streamed(command, cwd=ROOT, password=None):
    """Run an interactive command while retaining the output used for diagnosis."""
    command = [str(part) for part in command]
    print("+", " ".join(command), flush=True)
    if password is not None:
        return run_streamed_with_password(command, cwd, password)
    process = subprocess.Popen(
        command,
        cwd=cwd,
        stdin=None,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    chunks = []
    while True:
        chunk = os.read(process.stdout.fileno(), 4096)
        if not chunk:
            break
        chunks.append(chunk)
        output_buffer = getattr(sys.stdout, "buffer", None)
        if output_buffer is not None:
            output_buffer.write(chunk)
        else:
            sys.stdout.write(chunk.decode("utf-8", errors="replace"))
        sys.stdout.flush()
    returncode = process.wait()
    output = b"".join(chunks).decode("utf-8", errors="replace")
    if returncode:
        raise subprocess.CalledProcessError(returncode, command, output=output)
    return output


def normalized_terminal_output(data):
    text = data.decode("utf-8", errors="replace")
    return ANSI_ESCAPE.sub("", text).replace("\r", "")


def stop_process(process):
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


def prompt_for_steam_guard_code():
    try:
        code = getpass.getpass("Steam Guard code: ")
    except (EOFError, OSError) as exc:
        raise SteamInteractionError(
            "Steam Guard requested a code, but no interactive terminal is available"
        ) from exc
    if not code:
        raise SteamInteractionError("Steam Guard code cannot be empty")
    return code


def disable_terminal_echo(fd):
    settings = termios.tcgetattr(fd)
    settings[3] &= ~termios.ECHO
    termios.tcsetattr(fd, termios.TCSANOW, settings)


def run_streamed_with_password(command, cwd, password):
    """Answer SteamCMD's password prompt without exposing it in argv or logs."""
    master, slave = pty.openpty()
    disable_terminal_echo(slave)
    process = subprocess.Popen(
        command,
        cwd=cwd,
        stdin=slave,
        stdout=slave,
        stderr=slave,
        close_fds=True,
    )
    os.close(slave)
    chunks = []
    pending = b""
    password_sent = False
    prompt_deadline = time.monotonic() + LOGIN_PROMPT_TIMEOUT
    try:
        while True:
            readable, _, _ = select.select([master], [], [], 0.25)
            if master in readable:
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    chunk = b""
                if not chunk:
                    break
                chunks.append(chunk)
                output_buffer = getattr(sys.stdout, "buffer", None)
                if output_buffer is not None:
                    output_buffer.write(chunk)
                else:
                    sys.stdout.write(chunk.decode("utf-8", errors="replace"))
                sys.stdout.flush()
                pending = (pending + chunk)[-4096:]
                prompt_text = normalized_terminal_output(pending)
                if not password_sent and PASSWORD_PROMPT.search(prompt_text):
                    disable_terminal_echo(master)
                    os.write(master, password.encode("utf-8") + b"\n")
                    password_sent = True
                    pending = b""
                elif password_sent and STEAM_GUARD_PROMPT.search(prompt_text):
                    code = prompt_for_steam_guard_code()
                    disable_terminal_echo(master)
                    os.write(master, code.encode("utf-8") + b"\n")
                    pending = b""
            if process.poll() is not None and not readable:
                break
            if not password_sent and time.monotonic() >= prompt_deadline:
                raise SteamInteractionError(
                    "SteamCMD did not present a recognized password prompt within 60 seconds"
                )
    except BaseException:
        stop_process(process)
        raise
    finally:
        os.close(master)
    returncode = process.wait()
    output = b"".join(chunks).decode("utf-8", errors="replace")
    if returncode:
        raise subprocess.CalledProcessError(returncode, command, output=output)
    return output


def steam_log_snapshot(build_output):
    """Keep prior log bytes so appended output can exclude stale failures."""
    snapshot = {}
    if not build_output.is_dir():
        return snapshot
    for path in build_output.glob("*.log"):
        try:
            snapshot[path] = path.read_bytes()
        except OSError:
            continue
    return snapshot


def changed_steam_logs(build_output, before):
    changed = []
    if not build_output.is_dir():
        return changed
    for path in build_output.glob("*.log"):
        try:
            contents = path.read_bytes()
        except OSError:
            continue
        previous = before.get(path)
        if previous == contents:
            continue
        # SteamPipe may append rather than replace a log. In that case inspect
        # only bytes written by this attempt so an old HTTP 5xx cannot cause a
        # retry of a new, permanent failure.
        if previous is not None and contents.startswith(previous):
            contents = contents[len(previous):]
        changed.append((path, contents.decode("utf-8", errors="replace")))
    return changed


def transient_steampipe_http_statuses(logs):
    """Return HTTP 5xx statuses emitted by SteamPipe during the current attempt."""
    statuses = set()
    for _path, contents in logs:
        statuses.update(int(match.group(1)) for match in TRANSIENT_STEAMPIPE_HTTP.finditer(contents))
    return statuses


def upload_with_retry(command, build_output, attempts, retry_delay, password=None, job_label=""):
    transient_statuses = set()
    for attempt in range(1, attempts + 1):
        before = steam_log_snapshot(build_output)
        try:
            run_streamed(command, password=password)
            return
        except subprocess.CalledProcessError as exc:
            current_logs = changed_steam_logs(build_output, before)
            if exc.output:
                current_logs.append((None, exc.output))
            current_statuses = transient_steampipe_http_statuses(current_logs)
            # Exit code 6 covers both permanent SteamPipe rejection and temporary
            # CDN failures. Retry only when this attempt's logs prove HTTP 5xx.
            is_transient = exc.returncode == 6 and bool(current_statuses)
            transient_statuses.update(current_statuses)
            if not is_transient or attempt == attempts:
                raise SteamUploadError(
                    exc.returncode,
                    attempt,
                    transient_statuses if is_transient else (),
                    getattr(exc, "output", "") or "",
                    job_label,
                ) from exc
            delay = retry_delay * (2 ** (attempt - 1))
            statuses = ", ".join(f"HTTP {status}" for status in sorted(current_statuses))
            print(
                f"SteamPipe returned a temporary CDN error ({statuses}) on attempt "
                f"{attempt}/{attempts}; retrying the verified manifests in {delay:g} seconds...",
                file=sys.stderr,
                flush=True,
            )
            time.sleep(delay)


def git_value(*arguments, default="unknown"):
    try:
        return subprocess.run(
            ["git", *arguments],
            cwd=ROOT,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        ).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return default


def required_file(path, description):
    path = path.expanduser().resolve()
    if not path.is_file():
        raise SystemExit(f"Missing {description}: {path}")
    return path


def required_directory(path, description):
    path = path.expanduser().resolve()
    if not path.is_dir():
        raise SystemExit(f"Missing {description}: {path}")
    return path


def cmake_configured_cxx_compiler(build_dir):
    compiler_files = sorted(
        (build_dir / "CMakeFiles").glob("*/CMakeCXXCompiler.cmake"),
        key=lambda path: path.stat().st_mtime,
        reverse=True,
    )
    for compiler_file in compiler_files:
        contents = compiler_file.read_text(encoding="utf-8", errors="replace")
        match = re.search(r'^set\(CMAKE_CXX_COMPILER "([^"]+)"\)$', contents, re.MULTILINE)
        if match:
            return Path(match.group(1)).resolve()
    return None


def cmake_target_bits(build_dir):
    cache = build_dir / "CMakeCache.txt"
    contents = cache.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"^CMAKE_SIZEOF_VOID_P:INTERNAL=(4|8)$", contents, re.MULTILINE)
    if match:
        return {"4": "32", "8": "64"}[match.group(1)]
    compiler_files = sorted((build_dir / "CMakeFiles").glob("*/CMakeCCompiler.cmake"))
    for compiler_file in compiler_files:
        compiler_contents = compiler_file.read_text(encoding="utf-8", errors="replace")
        match = re.search(r'set\(CMAKE_C_SIZEOF_DATA_PTR "(4|8)"\)', compiler_contents)
        if match:
            return {"4": "32", "8": "64"}[match.group(1)]
    return None


def verify_steam_build(cache, description):
    contents = cache.read_text(encoding="utf-8", errors="replace")
    required = (
        "ENABLE_STEAMWORKS:BOOL=ON",
        "ENABLE_STEAM_GAMESERVER:BOOL=ON",
        "ENABLE_STEAM_LISTEN_SERVER:BOOL=ON",
        "ENABLE_LUA_MODS:BOOL=ON",
        "STEAM_APP_ID:STRING=1812700",
        "STEAM_PLAYTEST_APP_ID:STRING=1812730",
        "STEAM_GAMESERVER_APP_ID:STRING=5016790",
        "STEAM_MACOS_CLIENT_DEPOT_ID:STRING=1812704",
        "STEAM_MACOS_SERVER_DEPOT_ID:STRING=5016794",
    )
    missing = [setting for setting in required if setting not in contents]
    if missing:
        raise SystemExit(f"{description} is not a complete Steam release build; missing: {', '.join(missing)}")


def configure_steam_build(build_dir, sdk_root, windows, windows_bits="64"):
    if windows:
        if windows_bits == "32":
            compiler_names = ("i686-w64-mingw32-g++", "i686-w64-mingw32-g++-posix")
            toolchain_name = "mingw32.toolchain"
        else:
            compiler_names = ("x86_64-w64-mingw32-g++-posix",)
            toolchain_name = "mingw64.toolchain"
        compiler = next((shutil.which(name) for name in compiler_names if shutil.which(name)), None)
        if not compiler:
            raise SystemExit(f"Missing MinGW{windows_bits} C++ compiler: {compiler_names[0]}")
        cache = build_dir / "CMakeCache.txt"
        if cache.is_file():
            configured_compiler = cmake_configured_cxx_compiler(build_dir)
            if configured_compiler != Path(compiler).resolve():
                print(f"Removing incompatible Windows compiler cache: {build_dir}", flush=True)
                shutil.rmtree(build_dir)
    command = [
        "cmake",
        "-S", ROOT,
        "-B", build_dir,
        "-DCMAKE_BUILD_TYPE=Release",
        "-DENABLE_STEAMWORKS=ON",
        "-DENABLE_STEAM_GAMESERVER=ON",
        "-DENABLE_STEAM_LISTEN_SERVER=ON",
        "-DENABLE_LUA_MODS=ON",
        f"-DSTEAMWORKS_SDK_ROOT={sdk_root}",
        "-DSTEAM_APP_ID=1812700",
        "-DSTEAM_PLAYTEST_APP_ID=1812730",
        "-DSTEAM_GAMESERVER_APP_ID=5016790",
        "-DSTEAM_WINDOWS_CLIENT_DEPOT_ID=1812702",
        "-DSTEAM_LINUX_CLIENT_DEPOT_ID=1812703",
        "-DSTEAM_MACOS_CLIENT_DEPOT_ID=1812704",
        "-DSTEAM_WINDOWS_SERVER_DEPOT_ID=5016792",
        "-DSTEAM_LINUX_SERVER_DEPOT_ID=5016793",
        "-DSTEAM_MACOS_SERVER_DEPOT_ID=5016794",
    ]
    if windows:
        toolchain = required_file(ROOT / f"cmake/toolchains/{toolchain_name}", f"MinGW{windows_bits} CMake toolchain")
        command.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")
    run(command)
    cache = required_file(build_dir / "CMakeCache.txt", f"{'Windows' if windows else 'Linux'} CMake cache")
    verify_steam_build(cache, f"{'Windows' if windows else 'Linux'} build")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--linux-build-dir", default="build", help="Steam-enabled Linux CMake build directory")
    parser.add_argument("--windows-build-dir", default="build-windows-steam", help="Steam-enabled Windows CMake build directory")
    parser.add_argument("--windows-bits", choices=("32", "64"), help="Windows Steam build width when configuring locally (default: 64)")
    parser.add_argument("--macos-client-depot", help="Pre-staged macOS client depot from a macOS builder")
    parser.add_argument("--macos-server-depot", help="Pre-staged macOS dedicated server depot from a macOS builder")
    parser.add_argument("--platforms", default="windows,linux,macos",
                        help="Comma-separated depot platforms to publish (default: windows,linux,macos)")
    parser.add_argument("--sdk-root", default=os.environ.get("STEAMWORKS_SDK_ROOT", "~/sdk"))
    parser.add_argument("--output-root", default="dist/steam-release", help="Generated content, manifests and SteamPipe output")
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    parser.add_argument("--no-build", action="store_true", help="Use existing binaries without invoking CMake")
    parser.add_argument("--strict-assets", action="store_true", help="Require every shipped asset to be release-approved")
    parser.add_argument("--upload", action="store_true", help="Upload the selected app builds with SteamCMD after verification")
    parser.add_argument("--upload-target", choices=("all", "client", "server"), default="all", help="Select which verified app build to upload")
    parser.add_argument("--set-live", metavar="BRANCH", help="Set uploaded target builds live on this Steam branch (use default for public)")
    parser.add_argument("--playtest-app-id", default="1812730", help="Steam Playtest AppID that shares the client depots (default: 1812730)")
    parser.add_argument("--steam-account", default=os.environ.get("STEAM_ACCOUNT"), help="Steam partner account name")
    parser.add_argument("--steam-password-env", default="STEAM_PASSWORD",
                        help="Environment variable containing the Steam password for prompt automation")
    parser.add_argument("--steamcmd", default=os.environ.get("STEAMCMD", "steamcmd"))
    parser.add_argument("--upload-attempts", type=int, default=3, help="Attempts for proven transient SteamPipe HTTP 5xx failures (default: 3)")
    parser.add_argument("--upload-retry-delay", type=float, default=5.0, help="Initial retry delay in seconds; subsequent delays use exponential backoff")
    parser.add_argument("--standalone-linux-build-dir", help="Optional non-Steam build to verify")
    parser.add_argument("--standalone-windows-build-dir", help="Optional non-Steam build to verify")
    args = parser.parse_args()

    platforms = {platform.strip().lower() for platform in args.platforms.split(",") if platform.strip()}
    unknown_platforms = platforms - {"windows", "linux", "macos"}
    if not platforms or unknown_platforms:
        parser.error(f"invalid --platforms value: {args.platforms}")
    if "macos" in platforms and (not args.macos_client_depot or not args.macos_server_depot):
        parser.error("--platforms including macos requires --macos-client-depot and --macos-server-depot")

    if args.set_live and not re.fullmatch(r"[A-Za-z0-9_.-]{1,64}", args.set_live):
        raise SystemExit("--set-live must be a Steam branch name containing only letters, digits, '.', '_' or '-'")
    if args.set_live and args.set_live.lower() == "default":
        raise SystemExit(
            "SteamPipe cannot set the default branch live automatically. Upload without --set-live, "
            "then promote the Build to default from the Steamworks App Admin Builds page."
        )
    if args.set_live and not args.upload:
        raise SystemExit("--set-live requires --upload")
    if not 1 <= args.upload_attempts <= 10:
        raise SystemExit("--upload-attempts must be between 1 and 10")
    if not 0 <= args.upload_retry_delay <= 300:
        raise SystemExit("--upload-retry-delay must be between 0 and 300 seconds")

    linux_build = Path(args.linux_build_dir).expanduser().resolve()
    windows_build = Path(args.windows_build_dir).expanduser().resolve()
    macos_client_depot = required_directory(Path(args.macos_client_depot), "pre-staged macOS client depot") if "macos" in platforms else None
    macos_server_depot = required_directory(Path(args.macos_server_depot), "pre-staged macOS server depot") if "macos" in platforms else None
    sdk_root = Path(args.sdk_root).expanduser().resolve()
    output = Path(args.output_root).expanduser().resolve()
    content = output / "content"
    manifests = output / "manifests"
    build_output = output / "steampipe-output"

    linux_api = required_file(sdk_root / "redistributable_bin/linux64/libsteam_api.so", "Linux Steam API") if "linux" in platforms else None
    windows_bits = args.windows_bits or "64"
    if "windows" in platforms and args.no_build:
        windows_cache = required_file(windows_build / "CMakeCache.txt", "Windows CMake cache")
        configured_bits = cmake_target_bits(windows_build)
        if configured_bits:
            if args.windows_bits and args.windows_bits != configured_bits:
                raise SystemExit(
                    f"--windows-bits {args.windows_bits} does not match the Windows CMake cache ({configured_bits}-bit)"
                )
            windows_bits = configured_bits
    windows_api = required_file(
        sdk_root / ("redistributable_bin/steam_api.dll" if windows_bits == "32" else "redistributable_bin/win64/steam_api64.dll"),
        "Windows Steam API",
    ) if "windows" in platforms else None

    if args.no_build:
        if "linux" in platforms:
            linux_cache = required_file(linux_build / "CMakeCache.txt", "Linux CMake cache")
            verify_steam_build(linux_cache, "Linux build")
        if "windows" in platforms:
            windows_cache = required_file(windows_build / "CMakeCache.txt", "Windows CMake cache")
            verify_steam_build(windows_cache, "Windows build")
    else:
        if "linux" in platforms:
            configure_steam_build(linux_build, sdk_root, windows=False)
            run(["cmake", "--build", linux_build, "--parallel", args.jobs])
        if "windows" in platforms:
            configure_steam_build(windows_build, sdk_root, windows=True, windows_bits=windows_bits)
            run(["cmake", "--build", windows_build, "--parallel", args.jobs])

    stage_script = ROOT / "scripts/stage_steam_build.py"
    depots = {}
    if "linux" in platforms:
        depots.update({
            "linux-client": ("linux", "client", linux_build, linux_api),
            "linux-server": ("linux", "server", linux_build, linux_api),
        })
    if "windows" in platforms:
        depots.update({
            "windows-client": ("windows", "client", windows_build, windows_api),
            "windows-server": ("windows", "server", windows_build, windows_api),
        })
    for name, (platform, kind, build_dir, steam_api) in depots.items():
        run([
            sys.executable,
            stage_script,
            "--platform", platform,
            "--kind", kind,
            "--build-dir", build_dir,
            "--output", content / name,
            "--steam-api", steam_api,
        ])
    if "macos" in platforms:
        for name, source in (
            ("macos-client", macos_client_depot),
            ("macos-server", macos_server_depot),
        ):
            destination = content / name
            if destination.exists():
                shutil.rmtree(destination)
            shutil.copytree(source, destination)

    version = git_value("describe", "--tags", "--always", "--dirty", default="local")
    commit = git_value("rev-parse", "HEAD")
    client_set_live = (args.set_live or "") if args.upload_target in ("all", "client") else ""
    server_set_live = (args.set_live or "") if args.upload_target in ("all", "server") else ""
    if not re.fullmatch(r"[0-9]{1,10}", str(args.playtest_app_id)):
        raise SystemExit("--playtest-app-id must be a numeric Steam AppID")
    run([
        sys.executable,
        ROOT / "scripts/render_steam_build.py",
        "--platforms", ",".join(sorted(platforms)),
        "--output", manifests,
        "--build-output", build_output,
        "--content-root", content,
        "--windows-client-root", content / "windows-client",
        "--linux-client-root", content / "linux-client",
        "--macos-client-root", content / "macos-client",
        "--windows-server-root", content / "windows-server",
        "--linux-server-root", content / "linux-server",
        "--macos-server-root", content / "macos-server",
        "--version", version,
        "--git-commit", commit,
        "--client-set-live", client_set_live,
        "--playtest-set-live", "",
        "--server-set-live", server_set_live,
        "--playtest-app-id", args.playtest_app_id,
    ])

    verify = [
        sys.executable,
        ROOT / "scripts/verify_steam_release.py",
        "--manifests", manifests,
    ]
    for platform in sorted(platforms):
        verify.extend([f"--{platform}-client", content / f"{platform}-client"])
        verify.extend([f"--{platform}-server", content / f"{platform}-server"])
    for option, directory in (
        ("--standalone-linux", args.standalone_linux_build_dir),
        ("--standalone-windows", args.standalone_windows_build_dir),
    ):
        if not directory:
            continue
        build_dir = Path(directory).expanduser().resolve()
        suffix = ".exe" if option.endswith("windows") else ""
        verify.extend([f"{option}-client", build_dir / f"ninslash{suffix}"])
        verify.extend([f"{option}-server", build_dir / f"ninslash_srv{suffix}"])
    run(verify)

    if args.strict_assets:
        run([sys.executable, ROOT / "scripts/audit_release_assets.py", "--strict"])

    if not args.upload:
        print(f"Ready to upload. Manifests: {manifests}")
        return 0

    if not args.steam_account:
        raise SystemExit("--upload requires --steam-account or STEAM_ACCOUNT")
    steamcmd = shutil.which(args.steamcmd)
    if not steamcmd:
        raise SystemExit(f"SteamCMD executable not found: {args.steamcmd}")

    # Shared client depots are owned by the main AppID. SteamPipe cannot create
    # Playtest builds for them (FileMapping and DepotFromApp both fail server init).
    upload_jobs = []
    if args.upload_target in ("all", "client"):
        upload_jobs.append(("client", manifests / "app_build.vdf", build_output / "client"))
    if args.upload_target in ("all", "server"):
        upload_jobs.append(("server", manifests / "tool_build.vdf", build_output / "server"))

    if args.upload_target in ("all", "client"):
        print(
            "Playtest AppID 1812730 is not uploaded via SteamPipe (shared depots are owned by "
            "1812700). After the client Build succeeds, promote Playtest internal manually in "
            "Steamworks → App Admin → Builds.",
            flush=True,
        )

    password = os.environ.pop(args.steam_password_env, None) or None
    try:
        for label, manifest, job_output in upload_jobs:
            print(f"Uploading Steam {label} app build: {manifest}", flush=True)
            job_output.mkdir(parents=True, exist_ok=True)
            upload = [steamcmd, "+login", args.steam_account, "+run_app_build", manifest, "+quit"]
            upload_with_retry(
                upload, job_output, args.upload_attempts, args.upload_retry_delay,
                password=password, job_label=label,
            )
            # Password is only needed for interactive first login; subsequent
            # builds in this process use the cached SteamCMD session on disk.
            password = None
    except KeyboardInterrupt:
        raise SystemExit(
            f"SteamCMD upload interrupted. The verified manifests remain in {manifests}."
        ) from None
    except SteamInteractionError as exc:
        raise SystemExit(f"SteamCMD login failed: {exc}") from None
    except SteamUploadError as exc:
        log_hint = exc.output or ""
        if not log_hint and exc.__cause__ and getattr(exc.__cause__, "output", None):
            log_hint = str(exc.__cause__.output)
        if exc.transient_http_statuses:
            statuses = ", ".join(f"HTTP {status}" for status in exc.transient_http_statuses)
            detail = (
                f"SteamPipe's CDN still returned {statuses} after {exc.attempts} attempts while fetching or "
                "uploading depot data. The local file mapping and offline verification completed before upload; "
                "retry later. If the same manifest fails for an extended period, verify that the depot's previous "
                "manifest still exists in Steamworks and contact Steamworks Support."
            )
        elif exc.returncode == 6:
            detail = (
                "SteamPipe rejected the build. Check that the build account can edit and publish the target AppID, "
                "that its depots are saved and published, and that those depots belong to an account-owned package."
            )
            if exc.job_label == "client" and ("I/O Operation Failed" in log_hint or "initialize build on server" in log_hint):
                detail += (
                    " Check depot ownership/publish state and that the build account can edit depots "
                    "1812702/1812703/1812704 on AppID 1812700."
                )
            if args.set_live:
                detail += (
                    f" This upload also requested setlive={args.set_live!r}; changing a live branch requires "
                    "additional publish permission. Retry without --set-live to create the Build first, then "
                    "promote it in Steamworks after validation."
                )
        else:
            detail = "Fix SteamCMD login/network access and retry."
        raise SystemExit(
            f"SteamCMD upload failed for {exc.job_label or 'unknown target'} with exit code {exc.returncode}. "
            f"{detail} The verified manifests remain in {manifests}."
        ) from None
    print(f"SteamPipe upload completed. Output: {build_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
