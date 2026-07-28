#!/usr/bin/env python3
"""Offline validation for Ninslash SteamPipe content and standalone binaries."""

import argparse
import hashlib
import json
import os
import re
import shutil
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
WINDOWS_FORBIDDEN_RUNTIME_DLLS = {
    "msvcr100.dll", "msvcp100.dll", "atl100.dll", "mfc100.dll", "mfc100u.dll",
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


def inspect_windows_imports(path):
    objdump = shutil.which("x86_64-w64-mingw32-objdump")
    if objdump:
        output = command_output([objdump, "-p", str(path)]).lower()
        return output, set(re.findall(r"dll name:\s*([^\r\n]+)", output))
    try:
        import pefile
    except ImportError as exc:
        raise RuntimeError("install pefile or x86_64-w64-mingw32-objdump to inspect Windows dependencies") from exc
    pe = pefile.PE(str(path), fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
    imports = {
        entry.dll.decode("ascii", errors="replace").lower()
        for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", [])
    }
    architecture = ("pei-x86-64" if pe.FILE_HEADER.Machine == 0x8664 else f"machine-{pe.FILE_HEADER.Machine:#x}") + f" subsystem-{pe.OPTIONAL_HEADER.Subsystem}"
    return architecture, imports


def verify_windows_dependency_closure(root, entrypoints, errors):
    bundled = {path.name.lower(): path for path in root.glob("*.dll")}
    queue = list(entrypoints)
    visited = set()
    while queue:
        path = queue.pop(0)
        key = path.resolve()
        if key in visited:
            continue
        visited.add(key)
        try:
            output, imports = inspect_windows_imports(path)
        except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
            errors.append(f"{path}: dependency inspection failed: {exc}")
            continue
        if "pei-x86-64" not in output:
            errors.append(f"{path}: Windows release binary is not x86-64")
        forbidden = sorted(imports & WINDOWS_FORBIDDEN_RUNTIME_DLLS)
        if forbidden:
            errors.append(f"{path}: forbidden legacy VC runtime dependency: {', '.join(forbidden)}")
        missing = sorted(imports - WINDOWS_SYSTEM_DLLS - set(bundled))
        if missing:
            errors.append(f"{path}: missing imported runtime DLLs: {', '.join(missing)}")
        for imported in sorted(imports - WINDOWS_SYSTEM_DLLS):
            dependency = bundled.get(imported)
            if dependency:
                queue.append(dependency)


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
            image, imports = inspect_windows_imports(executable)
            if "steam_api64.dll" not in imports:
                errors.append(f"{executable}: missing steam_api64.dll import")
            is_client = executable.name.lower() == "ninslash.exe"
            if is_client and "windows gui" not in image.lower() and "subsystem-2" not in image.lower():
                errors.append(f"{executable}: Windows client must use the GUI subsystem")
            if not is_client and "windows cui" not in image.lower() and "subsystem-3" not in image.lower():
                errors.append(f"{executable}: Windows server must retain the console subsystem")
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        errors.append(f"{executable}: dependency inspection failed: {exc}")


def verify_depot(root_text, platform, kind, errors):
    if not root_text:
        return
    root = Path(root_text).resolve()
    suffix = ".exe" if platform == "windows" else ""
    executable_names = ["ninslash", "ninslash_srv"] if kind == "client" else ["ninslash_srv"]
    executables = [root / f"{name}{suffix}" for name in executable_names]
    steam_api = root / ("steam_api64.dll" if platform == "windows" else "libsteam_api.so")
    for required in ("data", "cfg", "autoexec.cfg", "storage.cfg"):
        path = root / required
        if not path.exists():
            errors.append(f"{root}: missing startup resource {required}")
    for executable in executables:
        if not executable.is_file():
            errors.append(f"{root}: missing {executable.name}")
    if not steam_api.is_file():
        errors.append(f"{root}: missing {steam_api.name}")
    if platform == "windows" and kind == "client" and not (root / "freetype-license.txt").is_file():
        errors.append(f"{root}: missing freetype-license.txt")
    verify_forbidden(root, errors)
    verify_inventory(root, errors)
    if not steam_api.is_file():
        return
    for executable in executables:
        if executable.is_file():
            verify_depot_executable(root, executable, steam_api, platform, errors)
    if platform == "windows":
        verify_windows_dependency_closure(root, [path for path in executables if path.is_file()], errors)


def verify_standalone(binary_text, platform, errors):
    if not binary_text:
        return
    binary = Path(binary_text).resolve()
    if not binary.is_file():
        errors.append(f"{binary}: standalone binary missing")
        return
    try:
        if platform == "linux":
            output = command_output(["ldd", str(binary)])
            imported_names = output.lower()
        else:
            output, imports = inspect_windows_imports(binary)
            imported_names = " ".join(imports)
        if "steam_api" in imported_names or "steamclient" in imported_names:
            errors.append(f"{binary}: standalone binary unexpectedly depends on Steam")
        if platform == "linux" and "not found" in output:
            errors.append(f"{binary}: unresolved Linux dependency")
        if platform == "windows":
            verify_windows_dependency_closure(binary.parent, [binary], errors)
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
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
