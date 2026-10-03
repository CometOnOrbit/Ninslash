# Foundry Warden artwork provenance

Foundry Warden is an altered, derivative design for Ninslash, not an asset shipped by the original authors.

The design was assembled from and matched to these existing Ninslash regions before the texture repaint:

| Repository source | Region | Use |
| --- | --- | --- |
| `data/anim/crawler.png` | `body_lower`, `body_top`, `eye` | Inner chassis, split armor, eye/core design |
| `data/anim/walker.png` | `Turret-Side-LowerLeg` | Armored leg design |
| `data/anim/stardroid.png` | `turret` | Mortar barrel design |
| `data/anim/jumppad.png` | `body` | Hazard-striped actuator housing |
| `data/anim/reactor.png` | lower half of `engine` | Heat-sink fins |
| `data/anim/teslacoil.png` | `top` | Reactor surround |

New matching jaw, hinge, brow, chin and foot shapes were added during authoring. An OpenAI-compatible image service then repainted the atlas using the requested model identifier `gpt-image-2.5-sunburst`, with the layout, original Crawler artwork and assembled design as references. The pinned RGBA source is `source/sunburst.png`; the generation prompt is recorded in `source/generation-prompt.txt`. Postprocessing removed the magenta matte, resized to 1024×1024, and restricted pixels to the atlas regions.

The motion pass derives additional heel/toe regions, separate luminous eye/core layers, and shaded rear-leg regions from that pinned texture. These operations and the animation bake are reproducible locally without an image service.

Source project: <https://github.com/CometOnOrbit/Ninslash>, authoring reference commit `1392f97`.

The repository's root `license.txt` states that content under `data/`, unless a more specific notice applies, is Ninslash content licensed under **CC BY-SA 3.0**. Ninslash copyright (c) 2016 Juho Syrjänen; Teeworlds copyright (c) 2007–2014 Magnus Auvinen. Preserve upstream attribution and any file-specific notices.

The derivative artwork, rig and previews are distributed under the same **CC BY-SA 3.0** terms: <https://creativecommons.org/licenses/by-sa/3.0/>. They are marked as modified work. The authoring, preview and validation code follow the repository's source-code license. The existing root `license.txt` remains unchanged and authoritative.
