#!/usr/bin/env python3

import pathlib
import re
import sys


def main():
    failed = False
    patterns = (
        (re.compile(r"QuadsEnd\s*\(\s*\)\s*;\s*Graphics\s*\(\s*\)\s*->\s*QuadsSetRotation", re.S), "QuadsSetRotation called after QuadsEnd"),
        (re.compile(r"Graphics\s*\(\s*\)\s*->\s*SetColor\s*\([^;]+;\s*RenderTools\s*\(\s*\)\s*->\s*RenderWeapon\s*\([^;]+true\s*\)", re.S), "SetColor called before self-batched RenderWeapon"),
        (re.compile(r"QuadsSetSubsetFree\s*\(\s*0\s*,\s*1\s*,\s*1\s*,\s*1\s*,\s*0\s*,\s*0\s*,\s*1\s*,\s*0\s*\)"), "malformed full-texture vertical flip"),
    )
    for path in pathlib.Path("src/game/client").rglob("*.cpp"):
        text = path.read_text(encoding="utf-8")
        for pattern, message in patterns:
            if pattern.search(text):
                print(f"{path}: {message}")
                failed = True
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
