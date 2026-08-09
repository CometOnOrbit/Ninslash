#!/usr/bin/env python3

import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

import publish_steam_depots


def main():
    with tempfile.TemporaryDirectory(prefix="ninslash-steam-render-") as temporary:
        temporary = Path(temporary)
        output = temporary / "manifests"
        content = temporary / "content"
        subprocess.run([
            sys.executable,
            ROOT / "scripts/render_steam_build.py",
            "--output", output,
            "--build-output", temporary / "output",
            "--content-root", content,
            "--platforms", "linux,windows",
            "--linux-client-root", content / "linux-client",
            "--linux-server-root", content / "linux-server",
            "--windows-client-root", content / "windows-client",
            "--windows-server-root", content / "windows-server",
            "--client-set-live", "internal",
            "--server-set-live", "internal",
        ], cwd=ROOT, check=True)

        names = {path.name for path in output.glob("*.vdf")}
        expected = {
            "app_build.vdf", "playtest_app_build.vdf", "tool_build.vdf",
            "depot_linux_client.vdf", "depot_linux_client_playtest.vdf",
            "depot_linux_server.vdf", "depot_windows_client.vdf",
            "depot_windows_client_playtest.vdf", "depot_windows_server.vdf",
        }
        if names != expected:
            raise SystemExit(f"unexpected Linux/Windows Steam manifests: {sorted(names)}")

        combined = "\n".join(path.read_text(encoding="utf-8") for path in output.glob("*.vdf"))
        for required in ("1812700", "1812730", "1812702", "1812703", "5016792", "5016793", '"setlive" "internal"'):
            if required not in combined:
                raise SystemExit(f"missing Linux/Windows Steam manifest value: {required}")
        playtest = (output / "playtest_app_build.vdf").read_text(encoding="utf-8")
        client = (output / "app_build.vdf").read_text(encoding="utf-8")
        playtest_windows = (output / "depot_windows_client_playtest.vdf").read_text(encoding="utf-8")
        if '"appid" "1812730"' not in playtest:
            raise SystemExit("playtest app build must target AppID 1812730 for documentation")
        if "depot_windows_client_playtest.vdf" not in playtest:
            raise SystemExit("playtest app build must reference DepotFromApp playtest depot manifests")
        if "depot_windows_client.vdf" in playtest:
            raise SystemExit("playtest app build must not FileMap-upload shared client depots")
        if '"setlive"' in playtest:
            raise SystemExit("playtest app build must not request setlive (Playtest is promoted manually)")
        if '"DepotFromApp" "1812700"' not in playtest_windows:
            raise SystemExit("playtest depot manifest must reference the main AppID via DepotFromApp")
        if "FileMapping" in playtest_windows:
            raise SystemExit("playtest depot manifest must not include FileMapping")
        if "1812702" not in client or "depot_windows_client.vdf" not in client:
            raise SystemExit("main client app build must upload shared Windows client depot")
        if "/client" not in playtest or "/client" not in client:
            raise SystemExit("playtest must reuse the client SteamPipe buildoutput cache")
        if playtest.count("/client") != 1 or client.count("/client") != 1:
            raise SystemExit("unexpected duplicate client buildoutput paths in manifests")
        for forbidden in ("1812704", "5016794", "depot_macos"):
            if forbidden in combined:
                raise SystemExit(f"Linux/Windows Steam manifests reference macOS: {forbidden}")

        compiler = temporary / "toolchain" / "x86_64-w64-mingw32-g++-posix"
        compiler.parent.mkdir()
        compiler.touch()
        metadata = temporary / "windows-build" / "CMakeFiles" / "3.25.1" / "CMakeCXXCompiler.cmake"
        metadata.parent.mkdir(parents=True)
        metadata.write_text(f'set(CMAKE_CXX_COMPILER "{compiler}")\n', encoding="utf-8")
        configured = publish_steam_depots.cmake_configured_cxx_compiler(temporary / "windows-build")
        if configured != compiler.resolve():
            raise SystemExit("matching MinGW CMake compiler metadata was not recognized")
        metadata.write_text('set(CMAKE_CXX_COMPILER "/usr/bin/other-g++")\n', encoding="utf-8")
        configured = publish_steam_depots.cmake_configured_cxx_compiler(temporary / "windows-build")
        if configured == compiler.resolve():
            raise SystemExit("incompatible MinGW CMake compiler metadata was accepted")


if __name__ == "__main__":
    main()
