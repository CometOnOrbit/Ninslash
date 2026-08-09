#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = []
# ///
# ─── How to run ───
# uv run tests/check_multibuffering_transition.py

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def active_cpp(source: str) -> str:
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.DOTALL)


def function_body(source: str, signature: str) -> str:
    match = re.search(rf"{re.escape(signature)}\s*\{{(?P<body>.*?)\n\}}", source, re.DOTALL)
    return match.group("body") if match else ""


def main() -> int:
    backend = active_cpp((ROOT / "src/engine/client/backend_sdl.cpp").read_text(encoding="utf-8"))
    graphics = active_cpp((ROOT / "src/engine/client/graphics_threaded.cpp").read_text(encoding="utf-8"))
    gameclient = active_cpp((ROOT / "src/game/client/gameclient.cpp").read_text(encoding="utf-8"))
    failures: list[str] = []

    if not re.search(
        r"if\s*\(\s*m_MultiBuffering\s*\)\s*\{.*?\}\s*else\s*\{.*?"
        r"glBindFramebuffer\s*\(\s*GL_FRAMEBUFFER\s*,\s*0\s*\).*?"
        r"glViewport\s*\(\s*0\s*,\s*0\s*,\s*m_ScreenWidth\s*,\s*m_ScreenHeight\s*\)",
        backend,
        flags=re.DOTALL,
    ):
        failures.append("OpenGL backend must restore the default framebuffer and screen viewport without FBOs")

    render_to_screen = function_body(graphics, "void CGraphics_Threaded::RenderToScreen()")
    if "m_GfxMultiBuffering" in render_to_screen:
        failures.append("RenderToScreen must update the target state when gfx_multibuffering is 0")

    for signature in (
        "void CGraphics_Threaded::CreateTextureBuffer(int Width, int Height)",
        "void CGraphics_Threaded::DestroyTextureBuffer()",
    ):
        body = function_body(graphics, signature)
        if "KickCommandBuffer();" not in body or "WaitForIdle();" not in body:
            failures.append(f"{signature} must complete its render-thread lifecycle before returning")

    render_start = gameclient.find("void CGameClient::OnRender()")
    render_end = gameclient.find("void CGameClient::OnRelease()", render_start)
    render_body = gameclient[render_start:render_end] if render_start >= 0 and render_end >= 0 else ""
    if not re.search(
        r"if\s*\(\s*g_Config\.m_GfxMultiBuffering\s*&&\s*!m_TextureBuffersCreated\s*\)"
        r"[\s\S]*?Graphics\(\)->CreateTextureBuffer\(Graphics\(\)->ScreenWidth\(\),"
        r"\s*Graphics\(\)->ScreenHeight\(\)\);"
        r"[\s\S]*?m_TextureBuffersCreated\s*=\s*true;"
        r"[\s\S]*?Graphics\(\)->ClearBufferTexture\(LightingBrightness\(\)\);",
        render_body,
    ):
        failures.append("OnRender must create and clear texture buffers after gfx_multibuffering is enabled")

    if failures:
        for failure in failures:
            print(f"multi-buffering transition: {failure}", file=sys.stderr)
        return 1
    print("multi-buffering transition: render target and FBO lifecycle contracts verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
