#!/usr/bin/env python3
"""Offline validation for Ninslash SteamPipe content and standalone binaries."""

import argparse
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path


EXPECTED_IDS = ("1812700", "1812702", "1812703", "5016790", "5016792", "5016793")
FORBIDDEN_NAMES = {
    "steam_appid.txt",
    "settings.cfg",
    "pve_progress.json",
    "steam_bans.cfg",
    "steam_pending_events.dat",
}
WINDOWS_SYSTEM_DLLS = {
    "advapi32.dll", "bcrypt.dll", "comdlg32.dll", "crypt32.dll", "dwmapi.dll",
    "gdi32.dll", "glu32.dll", "imm32.dll", "kernel32.dll", "msvcrt.dll",
    "ntdll.dll", "ole32.dll", "oleaut32.dll", "opengl32.dll", "rpcrt4.dll",
    "secur32.dll", "setupapi.dll", "shell32.dll", "shlwapi.dll", "user32.dll",
    "userenv.dll", "version.dll", "winmm.dll", "ws2_32.dll",
}
LINUX_SYSTEM_LIBRARIES = {
    "ld-linux-x86-64.so.2", "libc.so.6", "libdl.so.2", "libm.so.6",
    "libpthread.so.0", "librt.so.1", "libGL.so.1", "libGLX.so.0",
    "libOpenGL.so.0", "libX11.so.6", "libxcb.so.1", "libXau.so.6",
    "libXdmcp.so.6", "libbsd.so.0", "libmd.so.0",
}


def command_output(args):
    return subprocess.run(
        args, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, env=dict(os.environ, LC_ALL="C"),
    ).stdout


def verify_inventory(root, errors):
    manifest_path = root / "depot_manifest.json"
    if not manifest_path.is_file():
        errors.append(f"{root}: missing depot_manifest.json")
        return
    try:
        entries = json.loads(manifest_path.read_text(encoding="utf-8"))["files"]
    except (OSError, ValueError, KeyError, TypeError) as exc:
        errors.append(f"{manifest_path}: invalid inventory: {exc}")
        return
    listed = set()
    for entry in entries:
        relative = entry.get("path", "")
        path = root / relative
        listed.add(relative)
        if not path.is_file():
            errors.append(f"{root}: inventory file missing: {relative}")
            continue
        data = path.read_bytes()
        if len(data) != entry.get("size") or hashlib.sha256(data).hexdigest() != entry.get("sha256"):
            errors.append(f"{root}: inventory mismatch: {relative}")
    actual = {path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file() and path != manifest_path}
    if actual != listed:
        errors.append(f"{root}: inventory does not match depot files")


def verify_forbidden(root, errors):
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if path.name.lower() in FORBIDDEN_NAMES or path.suffix.lower() in {".log", ".dmp"}:
            errors.append(f"{root}: forbidden user/development file: {path.relative_to(root)}")


def verify_depot_executable(root, executable, steam_api, platform, errors):
    try:
        if platform == "linux":
            dynamic = command_output(["readelf", "-d", str(executable)])
            runpath_lines = [line for line in dynamic.splitlines() if "RPATH" in line or "RUNPATH" in line]
            if len(runpath_lines) != 1 or "[$ORIGIN]" not in runpath_lines[0] or "sdk" in runpath_lines[0].lower():
                errors.append(f"{executable}: RUNPATH must contain only $ORIGIN")
            linked = command_output(["ldd", str(executable)])
            expected_path = str(steam_api.resolve())
            if f"libsteam_api.so => {expected_path}" not in linked:
                errors.append(f"{executable}: libsteam_api.so is not resolved from its depot root")
            if "not found" in linked:
                errors.append(f"{executable}: unresolved Linux dependency")
            needed = set(re.findall(r"Shared library:\s*\[([^]]+)]", dynamic))
            bundled = needed - LINUX_SYSTEM_LIBRARIES
            depot_files = {path.name for path in root.iterdir() if path.is_file()}
            missing = sorted(bundled - depot_files)
            if missing:
                errors.append(f"{executable}: missing bundled Linux runtime libraries: {', '.join(missing)}")
            for library in sorted(bundled & depot_files):
                expected_library = str((root / library).resolve())
                if f"{library} => {expected_library}" not in linked:
                    errors.append(f"{executable}: {library} is not resolved from its depot root")
        else:
            imports = command_output(["x86_64-w64-mingw32-objdump", "-p", str(executable)]).lower()
            if "steam_api64.dll" not in imports:
                errors.append(f"{executable}: missing steam_api64.dll import")
            imported_dlls = set(re.findall(r"dll name:\s*([^\r\n]+)", imports))
            depot_dlls = {path.name.lower() for path in root.glob("*.dll")}
            missing = sorted(imported_dlls - WINDOWS_SYSTEM_DLLS - depot_dlls)
            if missing:
                errors.append(f"{executable}: missing imported runtime DLLs: {', '.join(missing)}")
    except (OSError, subprocess.CalledProcessError) as exc:
        errors.append(f"{executable}: dependency inspection failed: {exc}")


def verify_depot(root_text, platform, kind, errors):
    if not root_text:
        return
    root = Path(root_text).resolve()
    suffix = ".exe" if platform == "windows" else ""
    executable_names = ["ninslash", "ninslash_srv"] if kind == "client" else ["ninslash_srv"]
    executables = [root / f"{name}{suffix}" for name in executable_names]
    steam_api = root / ("steam_api64.dll" if platform == "windows" else "libsteam_api.so")
    for executable in executables:
        if not executable.is_file():
            errors.append(f"{root}: missing {executable.name}")
    if not steam_api.is_file():
        errors.append(f"{root}: missing {steam_api.name}")
    verify_forbidden(root, errors)
    verify_inventory(root, errors)
    if not steam_api.is_file():
        return
    for executable in executables:
        if executable.is_file():
            verify_depot_executable(root, executable, steam_api, platform, errors)


def verify_standalone(binary_text, platform, errors):
    if not binary_text:
        return
    binary = Path(binary_text).resolve()
    if not binary.is_file():
        errors.append(f"{binary}: standalone binary missing")
        return
    try:
        output = command_output(["ldd", str(binary)]) if platform == "linux" else command_output(["x86_64-w64-mingw32-objdump", "-p", str(binary)])
        if "steam_api" in output.lower() or "steamclient" in output.lower():
            errors.append(f"{binary}: standalone binary unexpectedly depends on Steam")
        if platform == "linux" and "not found" in output:
            errors.append(f"{binary}: unresolved Linux dependency")
    except (OSError, subprocess.CalledProcessError) as exc:
        errors.append(f"{binary}: standalone dependency inspection failed: {exc}")


def verify_vdfs(directory_text, errors):
    if not directory_text:
        return
    directory = Path(directory_text)
    files = sorted(directory.glob("*.vdf"))
    if not files:
        errors.append(f"{directory}: no rendered VDF files")
        return
    combined = "\n".join(path.read_text(encoding="utf-8") for path in files)
    if "@" in combined:
        errors.append(f"{directory}: unresolved VDF template token")
    if re.search(r'"setlive"\s+"(?:None|null)"', combined, re.IGNORECASE):
        errors.append(f"{directory}: invalid empty setlive value")
    for expected_id in EXPECTED_IDS:
        if f'"{expected_id}"' not in combined:
            errors.append(f"{directory}: missing assigned Steam ID {expected_id}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifests")
    parser.add_argument("--linux-client")
    parser.add_argument("--linux-server")
    parser.add_argument("--windows-client")
    parser.add_argument("--windows-server")
    parser.add_argument("--standalone-linux-client")
    parser.add_argument("--standalone-linux-server")
    parser.add_argument("--standalone-windows-client")
    parser.add_argument("--standalone-windows-server")
    args = parser.parse_args()

    errors = []
    verify_vdfs(args.manifests, errors)
    verify_depot(args.linux_client, "linux", "client", errors)
    verify_depot(args.linux_server, "linux", "server", errors)
    verify_depot(args.windows_client, "windows", "client", errors)
    verify_depot(args.windows_server, "windows", "server", errors)
    verify_standalone(args.standalone_linux_client, "linux", errors)
    verify_standalone(args.standalone_linux_server, "linux", errors)
    verify_standalone(args.standalone_windows_client, "windows", errors)
    verify_standalone(args.standalone_windows_server, "windows", errors)
    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        raise SystemExit(1)
    print("Steam release verification passed")


if __name__ == "__main__":
    main()
