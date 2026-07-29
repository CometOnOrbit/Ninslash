#!/usr/bin/env python3
"""Create a deterministic SteamPipe content root from a local release build."""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
from pathlib import Path

WINDOWS_SYSTEM_DLLS = {
    "advapi32.dll", "bcrypt.dll", "comdlg32.dll", "crypt32.dll", "dwmapi.dll",
    "gdi32.dll", "glu32.dll", "imm32.dll", "kernel32.dll", "msvcrt.dll",
    "ntdll.dll", "ole32.dll", "oleaut32.dll", "opengl32.dll", "rpcrt4.dll",
    "secur32.dll", "setupapi.dll", "shell32.dll", "shlwapi.dll", "user32.dll",
    "userenv.dll", "version.dll", "winmm.dll", "ws2_32.dll",
}
WINDOWS_FORBIDDEN_RUNTIME_DLLS = {"msvcr100.dll", "msvcp100.dll", "atl100.dll", "mfc100.dll", "mfc100u.dll"}


def copy_tree(source: Path, target: Path):
    if source.exists():
        shutil.copytree(source, target)


def copy_required(source: Path, target: Path):
    if not source.is_file():
        raise FileNotFoundError(source)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def copy_windows_runtime_dependencies(executable: Path, build_dir: Path, output: Path):
    local_dlls = {path.name.lower(): path for path in build_dir.glob("*.dll")}
    queue = [executable]
    visited = set()
    while queue:
        current = queue.pop(0)
        key = current.resolve()
        if key in visited:
            continue
        visited.add(key)
        inspection = subprocess.run(
            ["x86_64-w64-mingw32-objdump", "-p", str(current)],
            check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        ).stdout
        if "pei-x86-64" not in inspection.lower():
            raise RuntimeError(f"Windows release binary is not x86-64: {current}")
        imports = {match.group(1).lower() for match in re.finditer(r"DLL Name:\s*([^\r\n]+)", inspection, re.IGNORECASE)}
        forbidden = imports & WINDOWS_FORBIDDEN_RUNTIME_DLLS
        if forbidden:
            raise RuntimeError(f"forbidden legacy VC runtime imported by {current}: {', '.join(sorted(forbidden))}")
        for imported in sorted(imports - WINDOWS_SYSTEM_DLLS - {"steam_api64.dll"}):
            source = local_dlls.get(imported)
            if not source:
                raise RuntimeError(f"unable to resolve Windows runtime library {imported} for {current}")
            copy_required(source, output / source.name)
            queue.append(source)


LINUX_SYSTEM_LIBRARIES = {
    "ld-linux-x86-64.so.2", "libc.so.6", "libdl.so.2", "libm.so.6",
    "libpthread.so.0", "librt.so.1", "libGL.so.1", "libGLX.so.0",
    "libOpenGL.so.0", "libX11.so.6", "libxcb.so.1", "libXau.so.6",
    "libXdmcp.so.6", "libbsd.so.0", "libmd.so.0",
}


def copy_linux_runtime_dependencies(executable: Path, output: Path):
    environment = dict(os.environ, LC_ALL="C")
    dynamic = subprocess.run(
        ["readelf", "-d", str(executable)], check=True,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=environment,
    ).stdout
    needed = set(re.findall(r"Shared library:\s*\[([^]]+)]", dynamic))
    linked = subprocess.run(
        ["ldd", str(executable)], check=True,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=environment,
    ).stdout
    resolved = {
        match.group(1): Path(match.group(2))
        for match in re.finditer(r"^\s*(\S+)\s+=>\s+(/\S+)", linked, re.MULTILINE)
    }
    for library in sorted(needed - LINUX_SYSTEM_LIBRARIES - {"libsteam_api.so"}):
        source = resolved.get(library)
        if not source or not source.is_file():
            raise RuntimeError(
                f"unable to resolve Linux runtime library {library} for {executable}\n"
                f"ldd output:\n{linked.rstrip()}"
            )
        copy_required(source.resolve(), output / library)


def macos_dependencies(path: Path):
    inspection = subprocess.run(
        ["otool", "-L", str(path)], check=True,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
    ).stdout
    dependencies = []
    for line in inspection.splitlines()[1:]:
        match = re.match(r"\s*(\S+)\s+\(compatibility version", line)
        if match:
            dependencies.append(match.group(1))
    return dependencies


def copy_macos_runtime_dependencies(executables, build_dir: Path, output: Path, steam_api: Path | None):
    candidates = {path.name: path for path in build_dir.glob("*.dylib")}
    if steam_api:
        candidates[steam_api.name] = steam_api
    queue = list(executables)
    scheduled = {path.resolve() for path in executables}
    visited = set()
    copied_sources = {}
    while queue:
        current = queue.pop(0)
        resolved_current = current.resolve()
        if resolved_current in visited:
            continue
        visited.add(resolved_current)
        for dependency in macos_dependencies(current):
            if dependency.startswith(("/System/Library/", "/usr/lib/")):
                continue
            name = Path(dependency).name
            if current.suffix == ".dylib" and name == current.name:
                continue
            if dependency.startswith("@loader_path/"):
                source = current.parent / dependency.removeprefix("@loader_path/")
            elif dependency.startswith(("@rpath/", "@executable_path/")):
                source = candidates.get(name)
            else:
                source = Path(dependency)
            if not source or not source.is_file():
                raise RuntimeError(f"unable to resolve macOS runtime library {dependency} for {current}")
            source = source.resolve()
            target = output / name
            previous = copied_sources.get(name)
            if previous and previous != source:
                raise RuntimeError(f"conflicting macOS runtime libraries named {name}: {previous} and {source}")
            copied_sources[name] = source
            if not target.exists():
                copy_required(source, target)
            if target.resolve() not in scheduled:
                queue.append(target)
                scheduled.add(target.resolve())
            subprocess.run(
                ["install_name_tool", "-change", dependency, f"@loader_path/{name}", str(current)],
                check=True,
            )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", choices=("windows", "linux", "macos"), required=True)
    parser.add_argument("--kind", choices=("client", "server"), required=True)
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--output", required=True)
    parser.add_argument("--steam-api", help="Steam API runtime for the selected platform")
    args = parser.parse_args()

    root = Path.cwd()
    build_dir = Path(args.build_dir)
    output = Path(args.output)
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)

    executable_suffix = ".exe" if args.platform == "windows" else ""
    executable_names = ["ninslash", "ninslash_srv"] if args.kind == "client" else ["ninslash_srv"]
    staged_executables = []
    for executable in executable_names:
        executable_path = build_dir / f"{executable}{executable_suffix}"
        staged_executable = output / f"{executable}{executable_suffix}"
        copy_required(executable_path, staged_executable)
        staged_executables.append(staged_executable)
        if args.platform == "windows":
            copy_windows_runtime_dependencies(executable_path, build_dir, output)
        elif args.platform == "linux":
            copy_linux_runtime_dependencies(executable_path, output)
    copy_tree(root / "data", output / "data")
    copy_tree(root / "cfg", output / "cfg")
    for filename in ("autoexec.cfg", "autoexec_client.cfg", "storage.cfg", "license.txt", "THIRD_PARTY_LICENSES.md", "README.md", "README_zh-CN.md"):
        copy_required(root / filename, output / filename)
    if args.platform == "windows" and args.kind == "client":
        copy_required(root / "other/freetype/LICENSE.TXT", output / "freetype-license.txt")

    if args.steam_api:
        api = Path(args.steam_api)
        expected = {
            "windows": "steam_api64.dll",
            "linux": "libsteam_api.so",
            "macos": "libsteam_api.dylib",
        }[args.platform]
        if api.name != expected:
            raise ValueError(f"expected {expected}, got {api.name}")
        copy_required(api, output / expected)
    else:
        api = None

    if args.platform == "macos":
        copy_macos_runtime_dependencies(staged_executables, build_dir, output, api)

    forbidden = {"steam_appid.txt", "settings.cfg", "pve_progress.json", "steam_bans.cfg", "steam_pending_events.dat"}
    found_forbidden = [path for path in output.rglob("*") if path.is_file() and path.name.lower() in forbidden]
    found_forbidden.extend(path for path in output.rglob("*") if path.is_file() and path.suffix.lower() in {".log", ".dmp"})
    if found_forbidden:
        raise RuntimeError(f"forbidden development/user files in depot: {found_forbidden}")

    files = []
    for path in sorted(item for item in output.rglob("*") if item.is_file()):
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        files.append({"path": path.relative_to(output).as_posix(), "size": path.stat().st_size, "sha256": digest})
    (output / "depot_manifest.json").write_text(json.dumps({"files": files}, indent=2) + "\n", encoding="utf-8")
    print(f"Staged {args.kind} {args.platform} depot: {len(files)} files in {output}")


if __name__ == "__main__":
    main()
