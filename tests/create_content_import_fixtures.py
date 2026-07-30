#!/usr/bin/env python3

import pathlib
import json
import shutil
import sys
import tempfile
import zipfile

sys.path.insert(0, str(pathlib.Path.cwd()))
from scripts.content_hash import package_hash


def add_package(archive, source, prefix):
    for path in sorted(source.rglob("*")):
        if path.is_file():
            relative = path.relative_to(source).as_posix()
            archive.write(path, f"{prefix}{relative}")


def main():
    source = pathlib.Path(sys.argv[1])
    output = pathlib.Path(sys.argv[2])
    output.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(output / "friendly.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        add_package(archive, source, "plasma-carbine-example/")
    with zipfile.ZipFile(output / "root.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        add_package(archive, source, "")
    with zipfile.ZipFile(output / "traversal.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        add_package(archive, source, "plasma-carbine-example/")
        archive.writestr("plasma-carbine-example/../escape.txt", "escape")
    with zipfile.ZipFile(output / "undeclared.zip", "w", zipfile.ZIP_DEFLATED) as archive:
        add_package(archive, source, "")
        archive.writestr("extra.txt", "extra")
    with tempfile.TemporaryDirectory() as temporary:
        updated = pathlib.Path(temporary) / "updated"
        shutil.copytree(source, updated)
        manifest_path = updated / "ninslash_content.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        manifest["version"] = "2"
        manifest["content_hash"] = package_hash(updated, manifest)
        manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        with zipfile.ZipFile(output / "updated.zip", "w", zipfile.ZIP_DEFLATED) as archive:
            add_package(archive, updated, "plasma-carbine-example/")


if __name__ == "__main__":
    main()
