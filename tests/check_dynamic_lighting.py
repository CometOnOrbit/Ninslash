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
    polar_shader = (ROOT / "data/shaders/light_polar.frag").read_text(encoding="utf-8")
    backend = (ROOT / "src/engine/client/backend_sdl.cpp").read_text(encoding="utf-8")
    gameclient = (ROOT / "src/game/client/gameclient.cpp").read_text(encoding="utf-8")
    gameclient_header = (ROOT / "src/game/client/gameclient.h").read_text(encoding="utf-8")
    items = (ROOT / "src/game/client/components/items.cpp").read_text(encoding="utf-8")
    buildings = (ROOT / "src/game/client/components/buildings.cpp").read_text(encoding="utf-8")
    buildings2 = (ROOT / "src/game/client/components/buildings2.cpp").read_text(encoding="utf-8")

    checks = [
        (light, r"IsShaderAvailable\(SHADER_LIGHT\).*m_CollisionTexture", "shader availability and collision texture gating"),
        (light, r"RenderCpuVisibility[\s\S]*Collision\(\)->IntersectLine", "collision-aware CPU fallback"),
        (light, r"TextureSet\(g_pData->m_aImages\[Source\.m_Image\]\.m_Id\)[\s\S]*LightShaderBegin", "direct source-texture shader pass"),
        (light, r"LightShaderBegin[\s\S]*SetColor\(Source\.m_Color\.r, Source\.m_Color\.g, Source\.m_Color\.b, Source\.m_Color\.a\)", "source color forwarded to shadow shaders"),
        (shader, r"uniform sampler2D collision", "collision sampler"),
        (shader, r"RayVisibility[\s\S]*CollisionFlag", "tile-ray visibility"),
        (shader, r"texture2D\(texture,[^;]+\) \* gl_Color", "colored shadow-casting light sources"),
        (shader, r"Flag == 132[\s\S]*return 0", "platform transparency"),
        (shader, r"Flag == 129 \|\| Flag == 130\)\s*\n\s*return 1", "directional collision blocking"),
        (polar_shader, r"Visibility0[\s\S]*Visibility1[\s\S]*mix\(Visibility0, Visibility1, Blend\)", "visibility-space polar interpolation"),
        (polar_shader, r"texture2D\(texture,[^;]+\) \* gl_Color", "colored polar shadow light sources"),
        (light, r"MedianShadowDistance[\s\S]*aRawDistances", "polar shadow distance outlier filtering"),
        (light, r"!g_Config\.m_ClLighting && !m_pClient->DarkVisionEnabled\(\)", "DarkVision bypass of local cl_lighting"),
        (backend, r"m_aShader\[SHADER_LIGHT\].*light\.frag", "light shader loading"),
        (backend, r"glActiveTextureARB\(GL_TEXTURE1\)[\s\S]*glUniform1iARB\(location, 1\)", "collision texture unit binding"),
        (backend, r"pCommand->m_LightingBrightness", "server-authoritative lighting brightness"),
        (gameclient, r"ClearBufferTexture\(LightingBrightness\(\)\)", "effective lighting brightness passed to the render thread"),
        (gameclient_header, r"m_LocalLightingTarget|m_LocalLightingBrightness", "smooth client-side lighting transition state"),
        (gameclient, r"EnsureDarkVisionRenderBuffers\(\)[\s\S]*m_GfxMultiBuffering = 1", "DarkVision render buffers override local buffering preference"),
        (gameclient_header, r"m_ChallengeInfoReceived \? m_ChallengeVariantMask : 0", "no local Challenge fallback before server handshake"),
        (items, r"ProjectileLightWidth = 108\.0f[\s\S]*SimpleLight\([\s\S]*false\)", "visible non-shadowing projectile illumination"),
        (items, r"RenderLaser[\s\S]*BoxLight\(\s*LaserMid,[\s\S]*vec2\([^,]+,\s*LaserLength\s*\+\s*96\.0f\),\s*atan2\(Dir\.y,\s*Dir\.x\)\s*\+\s*pi\s*/\s*2,\s*false\)", "directional non-shadowing laser illumination"),
        (buildings, r"RenderScreen[\s\S]*vec2\(620, 400\)[\s\S]*RenderShop", "bright layered Screen illumination"),
        (buildings, r"RenderReactor[\s\S]*ReactorLightColor[\s\S]*SimpleLight[\s\S]*RenderTeslacoil", "bright Reactor illumination"),
        (buildings, r"GeneratorColor[\s\S]*SimpleLight", "team-colored Generator illumination"),
        (buildings2, r"GeneratorColor[\s\S]*SimpleLight", "team-colored Generator shield illumination"),
    ]

    Passed = all(require(text, pattern, description) for text, pattern, description in checks)
    if "m_ClChallengeVariants" in backend:
        print("dynamic lighting: render backend must not read local challenge variants")
        Passed = False
    return 0 if Passed else 1


if __name__ == "__main__":
    sys.exit(main())
