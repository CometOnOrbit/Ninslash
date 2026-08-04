#!/usr/bin/env python3

"""Static invariants for the dynamic-lighting render path.

The lighting pass has both a GLSL path and a fixed-function fallback.  These
checks intentionally inspect only the wiring that can regress without a GPU:
collision data must reach the shader, source textures must be sampled directly,
and the fallback must clip against collision visibility rather than a stale
full-screen light buffer.
"""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]


def require(text: str, pattern: str, description: str) -> bool:
    if re.search(pattern, text, re.MULTILINE | re.DOTALL):
        return True
    print(f"dynamic lighting: missing {description}")
    return False


def main() -> int:
    light = (ROOT / "src/game/client/components/light.cpp").read_text(encoding="utf-8")
    shader = (ROOT / "data/shaders/light.frag").read_text(encoding="utf-8")
    backend = (ROOT / "src/engine/client/backend_sdl.cpp").read_text(encoding="utf-8")
    gameclient = (ROOT / "src/game/client/gameclient.cpp").read_text(encoding="utf-8")
    gameclient_header = (ROOT / "src/game/client/gameclient.h").read_text(encoding="utf-8")

    checks = [
        (light, r"IsShaderAvailable\(SHADER_LIGHT\).*m_CollisionTexture", "shader availability and collision texture gating"),
        (light, r"RenderCpuVisibility[\s\S]*Collision\(\)->IntersectLine", "collision-aware CPU fallback"),
        (light, r"TextureSet\(g_pData->m_aImages\[Source\.m_Image\]\.m_Id\)[\s\S]*LightShaderBegin", "direct source-texture shader pass"),
        (shader, r"uniform sampler2D collision", "collision sampler"),
        (shader, r"RayVisibility[\s\S]*CollisionFlag", "tile-ray visibility"),
        (shader, r"Flag == 132[\s\S]*return 0", "platform transparency"),
        (shader, r"Flag == 129 \|\| Flag == 130\)\s*\n\s*return 1", "directional collision blocking"),
        (light, r"!g_Config\.m_ClLighting && !m_pClient->DarkVisionEnabled\(\)", "DarkVision bypass of local cl_lighting"),
        (backend, r"m_aShader\[SHADER_LIGHT\].*light\.frag", "light shader loading"),
        (backend, r"glActiveTextureARB\(GL_TEXTURE1\)[\s\S]*glUniform1iARB\(location, 1\)", "collision texture unit binding"),
        (backend, r"pCommand->m_DarkVision", "server-authoritative DarkVision clear flag"),
        (gameclient, r"ClearBufferTexture\(DarkVisionEnabled\(\)\)", "effective DarkVision passed to the render thread"),
        (gameclient, r"EnsureDarkVisionRenderBuffers\(\)[\s\S]*m_GfxMultiBuffering = 1", "DarkVision render buffers override local buffering preference"),
        (gameclient_header, r"m_ChallengeInfoReceived \? m_ChallengeVariantMask : 0", "no local Challenge fallback before server handshake"),
    ]

    Passed = all(require(text, pattern, description) for text, pattern, description in checks)
    if "m_ClChallengeVariants" in backend:
        print("dynamic lighting: render backend must not read local challenge variants")
        Passed = False
    return 0 if Passed else 1


if __name__ == "__main__":
    sys.exit(main())
