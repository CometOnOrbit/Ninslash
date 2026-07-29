#!/usr/bin/env python3
"""Build, stage, verify and optionally upload all Ninslash Steam depots."""

import argparse
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TRANSIENT_STEAMPIPE_HTTP = re.compile(r"\bHTTP\s+(5\d\d)\b", re.IGNORECASE)


class SteamUploadError(RuntimeError):
    def __init__(self, returncode, attempts, transient_http_statuses=()):
        super().__init__(f"SteamCMD exited with {returncode}")
        self.returncode = returncode
        self.attempts = attempts
        self.transient_http_statuses = tuple(sorted(set(transient_http_statuses)))


def run(command, cwd=ROOT):
    print("+", " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], cwd=cwd, check=True)


def run_streamed(command, cwd=ROOT):
    """Run an interactive command while retaining the output used for diagnosis."""
    command = [str(part) for part in command]
    print("+", " ".join(command), flush=True)
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


def upload_with_retry(command, build_output, attempts, retry_delay):
    transient_statuses = set()
    for attempt in range(1, attempts + 1):
        before = steam_log_snapshot(build_output)
        try:
            run_streamed(command)
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
                ) from None
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


def verify_steam_build(cache, description):
    contents = cache.read_text(encoding="utf-8", errors="replace")
    required = (
        "ENABLE_STEAMWORKS:BOOL=ON",
        "ENABLE_STEAM_GAMESERVER:BOOL=ON",
        "ENABLE_STEAM_LISTEN_SERVER:BOOL=ON",
        "ENABLE_LUA_MODS:BOOL=ON",
        "STEAM_APP_ID:STRING=1812700",
        "STEAM_GAMESERVER_APP_ID:STRING=5016790",
        "STEAM_MACOS_CLIENT_DEPOT_ID:STRING=1812704",
        "STEAM_MACOS_SERVER_DEPOT_ID:STRING=5016794",
    )
    missing = [setting for setting in required if setting not in contents]
    if missing:
        raise SystemExit(f"{description} is not a complete Steam release build; missing: {', '.join(missing)}")


def configure_steam_build(build_dir, sdk_root, windows):
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
        "-DSTEAM_GAMESERVER_APP_ID=5016790",
        "-DSTEAM_WINDOWS_CLIENT_DEPOT_ID=1812702",
        "-DSTEAM_LINUX_CLIENT_DEPOT_ID=1812703",
        "-DSTEAM_MACOS_CLIENT_DEPOT_ID=1812704",
        "-DSTEAM_WINDOWS_SERVER_DEPOT_ID=5016792",
        "-DSTEAM_LINUX_SERVER_DEPOT_ID=5016793",
        "-DSTEAM_MACOS_SERVER_DEPOT_ID=5016794",
    ]
    if windows:
        toolchain = required_file(ROOT / "cmake/toolchains/mingw64.toolchain", "MinGW64 CMake toolchain")
        if not shutil.which("x86_64-w64-mingw32-g++"):
            raise SystemExit("Missing MinGW64 compiler: x86_64-w64-mingw32-g++")
        command.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")
    run(command)
    cache = required_file(build_dir / "CMakeCache.txt", f"{'Windows' if windows else 'Linux'} CMake cache")
    verify_steam_build(cache, f"{'Windows' if windows else 'Linux'} build")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--linux-build-dir", default="build", help="Steam-enabled Linux CMake build directory")
    parser.add_argument("--windows-build-dir", default="build-windows-steam", help="Steam-enabled Windows CMake build directory")
    parser.add_argument("--macos-client-depot", required=True, help="Pre-staged macOS client depot from a macOS builder")
    parser.add_argument("--macos-server-depot", required=True, help="Pre-staged macOS dedicated server depot from a macOS builder")
    parser.add_argument("--sdk-root", default=os.environ.get("STEAMWORKS_SDK_ROOT", "~/sdk"))
    parser.add_argument("--output-root", default="dist/steam-release", help="Generated content, manifests and SteamPipe output")
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() or 1))
    parser.add_argument("--no-build", action="store_true", help="Use existing binaries without invoking CMake")
    parser.add_argument("--strict-assets", action="store_true", help="Require every shipped asset to be release-approved")
    parser.add_argument("--upload", action="store_true", help="Upload the selected app builds with SteamCMD after verification")
    parser.add_argument("--upload-target", choices=("all", "client", "server"), default="all", help="Select which verified app build to upload")
    parser.add_argument("--set-live", metavar="BRANCH", help="Set uploaded target builds live on this Steam branch (use default for public)")
    parser.add_argument("--steam-account", default=os.environ.get("STEAM_ACCOUNT"), help="Steam partner account name; password is never accepted here")
    parser.add_argument("--steamcmd", default=os.environ.get("STEAMCMD", "steamcmd"))
    parser.add_argument("--upload-attempts", type=int, default=3, help="Attempts for proven transient SteamPipe HTTP 5xx failures (default: 3)")
    parser.add_argument("--upload-retry-delay", type=float, default=5.0, help="Initial retry delay in seconds; subsequent delays use exponential backoff")
    parser.add_argument("--standalone-linux-build-dir", help="Optional non-Steam build to verify")
    parser.add_argument("--standalone-windows-build-dir", help="Optional non-Steam build to verify")
    args = parser.parse_args()

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
    macos_client_depot = required_directory(Path(args.macos_client_depot), "pre-staged macOS client depot")
    macos_server_depot = required_directory(Path(args.macos_server_depot), "pre-staged macOS server depot")
    sdk_root = Path(args.sdk_root).expanduser().resolve()
    output = Path(args.output_root).expanduser().resolve()
    content = output / "content"
    manifests = output / "manifests"
    build_output = output / "steampipe-output"

    linux_api = required_file(sdk_root / "redistributable_bin/linux64/libsteam_api.so", "Linux Steam API")
    windows_api = required_file(sdk_root / "redistributable_bin/win64/steam_api64.dll", "Windows Steam API")

    if args.no_build:
        linux_cache = required_file(linux_build / "CMakeCache.txt", "Linux CMake cache")
        windows_cache = required_file(windows_build / "CMakeCache.txt", "Windows CMake cache")
        verify_steam_build(linux_cache, "Linux build")
        verify_steam_build(windows_cache, "Windows build")
    else:
        configure_steam_build(linux_build, sdk_root, windows=False)
        configure_steam_build(windows_build, sdk_root, windows=True)
        run(["cmake", "--build", linux_build, "--parallel", args.jobs])
        run(["cmake", "--build", windows_build, "--parallel", args.jobs])

    stage_script = ROOT / "scripts/stage_steam_build.py"
    depots = {
        "linux-client": ("linux", "client", linux_build, linux_api),
        "linux-server": ("linux", "server", linux_build, linux_api),
        "windows-client": ("windows", "client", windows_build, windows_api),
        "windows-server": ("windows", "server", windows_build, windows_api),
    }
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
    run([
        sys.executable,
        ROOT / "scripts/render_steam_build.py",
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
        "--server-set-live", server_set_live,
    ])

    verify = [
        sys.executable,
        ROOT / "scripts/verify_steam_release.py",
        "--manifests", manifests,
        "--linux-client", content / "linux-client",
        "--linux-server", content / "linux-server",
        "--windows-client", content / "windows-client",
        "--windows-server", content / "windows-server",
        "--macos-client", content / "macos-client",
        "--macos-server", content / "macos-server",
    ]
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
    upload = [steamcmd, "+login", args.steam_account]
    if args.upload_target in ("all", "client"):
        upload.extend(["+run_app_build", manifests / "app_build.vdf"])
    if args.upload_target in ("all", "server"):
        upload.extend(["+run_app_build", manifests / "tool_build.vdf"])
    upload.append("+quit")
    try:
        upload_with_retry(upload, build_output, args.upload_attempts, args.upload_retry_delay)
    except SteamUploadError as exc:
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
            if args.set_live:
                detail += (
                    f" This upload also requested setlive={args.set_live!r}; changing a live branch requires "
                    "additional publish permission. Retry without --set-live to create the Build first, then "
                    "promote it in Steamworks after validation."
                )
        else:
            detail = "Fix SteamCMD login/network access and retry."
        raise SystemExit(
            f"SteamCMD upload failed with exit code {exc.returncode}. "
            f"{detail} The verified manifests remain in {manifests}."
        ) from None
    print(f"SteamPipe upload completed. Output: {build_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
