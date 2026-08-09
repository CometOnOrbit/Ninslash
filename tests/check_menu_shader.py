#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = []
# ///
# ─── How to run ───
# uv run tests/check_menu_shader.py

import re
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    shader = (ROOT / "data/shaders/menu.frag").read_text(encoding="utf-8")
    menus = (ROOT / "src/game/client/components/menus.cpp").read_text(encoding="utf-8")
    failures: list[str] = []

    if re.search(r"\bfloat\s+noise2\s*\(", shader):
        failures.append("menu shader helper shadows the GLSL noise2 built-in")
    if not re.search(r"\bfloat\s+menuNoise2\s*\(", shader):
        failures.append("menu shader must define the portable menuNoise2 helper")
    if not re.search(
        r"const bool UseMenuShader[\s\S]*IsShaderAvailable\(SHADER_MENU\)",
        menus,
    ):
        failures.append("menu background must guard FBO rendering by SHADER_MENU availability")
    if not re.search(
        r"\}\s*\n\s*Graphics\(\)->RenderToScreen\(\);\s*\n\s*// render background color",
        menus,
    ):
        failures.append("menu background fallback must restore the screen render target")

    validator = shutil.which("glslangValidator")
    if validator:
        result = subprocess.run(
            [validator, "-S", "frag", str(ROOT / "data/shaders/menu.frag")],
            cwd=ROOT,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        if result.returncode:
            failures.append(f"strict GLSL validation failed:\n{result.stdout.rstrip()}")

    if failures:
        for failure in failures:
            print(f"menu shader: {failure}", file=sys.stderr)
        return 1
    print("menu shader: portable helper and runtime fallback guard verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
