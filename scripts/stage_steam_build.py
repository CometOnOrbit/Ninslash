#!/usr/bin/env python3
"""Create a deterministic SteamPipe content root from a local release build."""

import argparse
import hashlib
import json
import shutil
from pathlib import Path


def copy_tree(source: Path, target: Path):
    if source.exists():
        shutil.copytree(source, target)


def copy_required(source: Path, target: Path):
    if not source.is_file():
        raise FileNotFoundError(source)
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, target)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", choices=("windows", "linux"), required=True)
    parser.add_argument("--kind", choices=("client", "server"), required=True)
    parser.add_argument("--build-dir", default="build")
    parser.add_argument("--output", required=True)
    parser.add_argument("--steam-api", help="steam_api64.dll or libsteam_api.so for a Steam client build")
    args = parser.parse_args()

    root = Path.cwd()
    build_dir = Path(args.build_dir)
    output = Path(args.output)
    if output.exists():
        shutil.rmtree(output)
    output.mkdir(parents=True)

    executable_suffix = ".exe" if args.platform == "windows" else ""
    executable = "ninslash" if args.kind == "client" else "ninslash_srv"
    copy_required(build_dir / f"{executable}{executable_suffix}", output / f"{executable}{executable_suffix}")
    copy_tree(root / "data", output / "data")
    copy_tree(root / "cfg", output / "cfg")
    for filename in ("autoexec.cfg", "storage.cfg", "license.txt", "THIRD_PARTY_LICENSES.md", "README.md", "README_zh-CN.md"):
        copy_required(root / filename, output / filename)

    if args.steam_api:
        if args.kind != "client":
            raise ValueError("--steam-api is only valid for a client depot")
        api = Path(args.steam_api)
        expected = "steam_api64.dll" if args.platform == "windows" else "libsteam_api.so"
        if api.name != expected:
            raise ValueError(f"expected {expected}, got {api.name}")
        copy_required(api, output / expected)

    forbidden = {"steam_appid.txt", "settings.cfg", "pve_progress.json"}
    found_forbidden = [path for path in output.rglob("*") if path.is_file() and path.name.lower() in forbidden]
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
