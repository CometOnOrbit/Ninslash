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
from typing import Literal


APP_IDS = ("1812700", "5016790")
PLATFORM_DEPOT_IDS = {
    "windows": ("1812702", "5016792"),
    "linux": ("1812703", "5016793"),
    "macos": ("1812704", "5016794"),
}
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

LUA_MOD_RUNTIME_SIGNATURES = (
    b"mod instruction budget exceeded",
    b"unable to activate Lua Mod",
)
LUA_MOD_DISABLED_SIGNATURE = b"server was built without Lua Mod support"

STEAM_INPUT_ACTIONS = (
    "confirm", "cancel", "fire", "turbo", "scoreboard", "build", "drop", "emote",
    "weapon_picker", "last_weapon", "prev_weapon", "next_weapon", "up", "down", "left",
    "right", "jump", "crouch", "charge", "inventory", "forge", "drone_radial", "weapon_1",
    "weapon_2", "weapon_3", "weapon_4", "ready", "vote_yes", "vote_no", "chat", "pause",
    "replay_play_pause", "replay_seek_back", "replay_seek_forward", "editor_primary", "editor_secondary",
)
STEAM_INPUT_LAYERS = ("menu", "spectator", "chat", "inventory", "build", "radial_menu", "replay", "editor")


def command_output(args):
    return subprocess.run(
        args, check=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, env=dict(os.environ, LC_ALL="C"),
    ).stdout


def parse_vdf(text):
    tokens = []
    position = 0
    while position < len(text):
        if text[position].isspace():
            position += 1
            continue
        if text.startswith("//", position):
            newline = text.find("\n", position)
            position = len(text) if newline < 0 else newline + 1
            continue
        if text[position] in "{}":
            tokens.append(text[position])
            position += 1
            continue
        if text[position] != '"':
            raise ValueError(f"unexpected character at byte {position}")
        position += 1
        value = []
        while position < len(text) and text[position] != '"':
            if text[position] == "\\" and position + 1 < len(text):
                position += 1
            value.append(text[position])
            position += 1
        if position >= len(text):
            raise ValueError("unterminated string")
        position += 1
        tokens.append("".join(value))

    def parse_pairs(index, nested):
        pairs = []
        while index < len(tokens):
            if tokens[index] == "}":
                if not nested:
                    raise ValueError("unexpected closing brace")
                return pairs, index + 1
            if tokens[index] == "{":
                raise ValueError("missing key before opening brace")
            key = tokens[index]
            index += 1
            if index >= len(tokens):
                raise ValueError(f"missing value for {key}")
            if tokens[index] == "{":
                value, index = parse_pairs(index + 1, True)
            elif tokens[index] == "}":
                raise ValueError(f"missing value for {key}")
            else:
                value = tokens[index]
                index += 1
            pairs.append((key, value))
        if nested:
            raise ValueError("missing closing brace")
        return pairs, index

    pairs, consumed = parse_pairs(0, False)
    if consumed != len(tokens):
        raise ValueError("trailing VDF tokens")
    return pairs


def vdf_block(pairs, key):
    for candidate, value in pairs:
        if candidate == key and isinstance(value, list):
            return value
    return None


def verify_steam_input_manifest(root, errors):
    path = root / "data/steam_input_manifest.vdf"
    if not path.is_file():
        errors.append(f"{root}: missing Steam Input action manifest")
        return
    try:
        top = parse_vdf(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, ValueError) as exc:
        errors.append(f"{path}: invalid VDF: {exc}")
        return
    manifest = vdf_block(top, "Action Manifest")
    if manifest is None or len(top) != 1:
        errors.append(f"{path}: root must be a single Action Manifest block")
        return
    actions = vdf_block(manifest, "actions")
    gameplay = vdf_block(actions or [], "gameplay")
    buttons = vdf_block(gameplay or [], "Button")
    analog = vdf_block(gameplay or [], "StickPadGyro")
    declared_actions = {key for key, value in buttons or [] if isinstance(value, str)}
    missing_actions = sorted(set(STEAM_INPUT_ACTIONS) - declared_actions)
    if missing_actions:
        errors.append(f"{path}: missing digital actions: {', '.join(missing_actions)}")
    declared_analog = {key for key, value in analog or [] if isinstance(value, list)}
    if not {"move", "aim"}.issubset(declared_analog):
        errors.append(f"{path}: move and aim analog actions are required")
    layers = vdf_block(manifest, "action_layers")
    declared_layers = {key for key, value in layers or [] if isinstance(value, list)}
    missing_layers = sorted(set(STEAM_INPUT_LAYERS) - declared_layers)
    if missing_layers:
        errors.append(f"{path}: missing action layers: {', '.join(missing_layers)}")
    localization = vdf_block(manifest, "localization")
    declared_languages = {key for key, value in localization or [] if isinstance(value, list)}
    missing_languages = sorted({"english", "schinese", "tchinese"} - declared_languages)
    if missing_languages:
        errors.append(f"{path}: missing Steam Input localization: {', '.join(missing_languages)}")


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


def verify_lua_mod_runtime(executable, errors):
    try:
        contents = executable.read_bytes()
    except OSError as exc:
        errors.append(f"{executable}: unable to inspect Lua Mod runtime: {exc}")
        return
    if LUA_MOD_DISABLED_SIGNATURE in contents:
        errors.append(f"{executable}: built without Lua Mod support")
        return
    missing = [signature.decode("ascii") for signature in LUA_MOD_RUNTIME_SIGNATURES if signature not in contents]
    if missing:
        errors.append(f"{executable}: Lua Mod runtime is incomplete; missing binary signatures: {', '.join(missing)}")


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
        elif platform == "windows":
            image, imports = inspect_windows_imports(executable)
            if "steam_api64.dll" not in imports:
                errors.append(f"{executable}: missing steam_api64.dll import")
            is_client = executable.name.lower() == "ninslash.exe"
            if is_client and "windows gui" not in image.lower() and "subsystem-2" not in image.lower():
                errors.append(f"{executable}: Windows client must use the GUI subsystem")
            if not is_client and "windows cui" not in image.lower() and "subsystem-3" not in image.lower():
                errors.append(f"{executable}: Windows server must retain the console subsystem")
        else:
            file_description = command_output(["file", str(executable)])
            if "Mach-O" not in file_description or not any(architecture in file_description for architecture in ("arm64", "x86_64")):
                errors.append(f"{executable}: macOS release binary must be 64-bit Mach-O")
            is_entrypoint = executable.name in {"ninslash", "ninslash_srv"}
            otool = shutil.which("otool")
            if not otool:
                if is_entrypoint and b"libsteam_api.dylib" not in executable.read_bytes():
                    errors.append(f"{executable}: missing libsteam_api.dylib reference")
                return
            inspection = command_output([otool, "-L", str(executable)])
            dependencies = []
            for line in inspection.splitlines()[1:]:
                match = re.match(r"\s*(\S+)\s+\(compatibility version", line)
                if match:
                    dependencies.append(match.group(1))
            if is_entrypoint and "@loader_path/libsteam_api.dylib" not in dependencies:
                errors.append(f"{executable}: libsteam_api.dylib must resolve through @loader_path")
            for dependency in dependencies:
                if dependency.startswith(("/System/Library/", "/usr/lib/")):
                    continue
                if executable.suffix == ".dylib" and Path(dependency).name == executable.name:
                    continue
                if not dependency.startswith("@loader_path/"):
                    errors.append(f"{executable}: non-system macOS dependency is not portable: {dependency}")
                    continue
                if not (root / dependency.removeprefix("@loader_path/")).is_file():
                    errors.append(f"{executable}: missing bundled macOS dependency: {dependency}")
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        errors.append(f"{executable}: dependency inspection failed: {exc}")


def verify_depot(root_text, platform, kind, errors):
    if not root_text:
        return
    root = Path(root_text).resolve()
    suffix = ".exe" if platform == "windows" else ""
    executable_names = ["ninslash", "ninslash_srv"] if kind == "client" else ["ninslash_srv"]
    executables = [root / f"{name}{suffix}" for name in executable_names]
    steam_api = root / {
        "windows": "steam_api64.dll",
        "linux": "libsteam_api.so",
        "macos": "libsteam_api.dylib",
    }[platform]
    for required in ("data", "cfg", "autoexec.cfg", "autoexec_client.cfg", "storage.cfg"):
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
    if kind == "client":
        verify_steam_input_manifest(root, errors)
    if not steam_api.is_file():
        return
    for executable in executables:
        if executable.is_file():
            verify_lua_mod_runtime(executable, errors)
            verify_depot_executable(root, executable, steam_api, platform, errors)
    if platform == "windows":
        verify_windows_dependency_closure(root, [path for path in executables if path.is_file()], errors)
    elif platform == "macos":
        for library in root.glob("*.dylib"):
            verify_depot_executable(root, library, steam_api, platform, errors)


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


def verify_steam_windows_binary(binary_text: str | None, kind: Literal["client", "server"], errors: list[str]) -> None:
    if not binary_text:
        return
    binary = Path(binary_text).resolve()
    if not binary.is_file():
        errors.append(f"{binary}: Steam Windows binary missing")
        return
    steam_api = binary.parent / "steam_api64.dll"
    if not steam_api.is_file():
        errors.append(f"{binary.parent}: missing steam_api64.dll")
    try:
        image, imports = inspect_windows_imports(binary)
        if "pei-x86-64" not in image:
            errors.append(f"{binary}: Windows Steam binary is not x86-64")
        if "steam_api64.dll" not in imports:
            errors.append(f"{binary}: missing steam_api64.dll import")
        expected_subsystems, description = {
            "client": (("windows gui", "subsystem-2"), "GUI"),
            "server": (("windows cui", "subsystem-3"), "console"),
        }[kind]
        if not any(subsystem in image for subsystem in expected_subsystems):
            errors.append(f"{binary}: Windows {kind} must use the {description} subsystem")
        verify_windows_dependency_closure(binary.parent, [binary], errors)
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        errors.append(f"{binary}: Steam Windows dependency inspection failed: {exc}")


def verify_vdfs(directory_text, platforms, errors):
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
    expected_ids = list(APP_IDS)
    for platform in sorted(platforms):
        expected_ids.extend(PLATFORM_DEPOT_IDS[platform])
    for expected_id in expected_ids:
        if f'"{expected_id}"' not in combined:
            errors.append(f"{directory}: missing assigned Steam ID {expected_id}")
    for platform in set(PLATFORM_DEPOT_IDS) - set(platforms):
        for omitted_id in PLATFORM_DEPOT_IDS[platform]:
            if f'"{omitted_id}"' in combined:
                errors.append(f"{directory}: contains unselected {platform} DepotID {omitted_id}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifests")
    parser.add_argument("--linux-client")
    parser.add_argument("--linux-server")
    parser.add_argument("--windows-client")
    parser.add_argument("--windows-server")
    parser.add_argument("--macos-client")
    parser.add_argument("--macos-server")
    parser.add_argument("--standalone-linux-client")
    parser.add_argument("--standalone-linux-server")
    parser.add_argument("--standalone-windows-client")
    parser.add_argument("--standalone-windows-server")
    parser.add_argument("--steam-windows-client")
    parser.add_argument("--steam-windows-server")
    args = parser.parse_args()

    errors = []
    platforms = {
        platform
        for platform in ("linux", "windows", "macos")
        if getattr(args, f"{platform}_client") or getattr(args, f"{platform}_server")
    }
    verify_vdfs(args.manifests, platforms, errors)
    verify_depot(args.linux_client, "linux", "client", errors)
    verify_depot(args.linux_server, "linux", "server", errors)
    verify_depot(args.windows_client, "windows", "client", errors)
    verify_depot(args.windows_server, "windows", "server", errors)
    verify_depot(args.macos_client, "macos", "client", errors)
    verify_depot(args.macos_server, "macos", "server", errors)
    verify_standalone(args.standalone_linux_client, "linux", errors)
    verify_standalone(args.standalone_linux_server, "linux", errors)
    verify_standalone(args.standalone_windows_client, "windows", errors)
    verify_standalone(args.standalone_windows_server, "windows", errors)
    verify_steam_windows_binary(args.steam_windows_client, "client", errors)
    verify_steam_windows_binary(args.steam_windows_server, "server", errors)
    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        raise SystemExit(1)
    print("Steam release verification passed")


if __name__ == "__main__":
    main()
