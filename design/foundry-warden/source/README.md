# Pinned authoring inputs

- `reference-rig.json`: the reference region skeleton and its original animation tracks; also used by the preview comparison.
- `reference-timing.json`: reference animation names and timing metadata.
- `sunburst.png` / `sunburst.atlas`: the 16-region RGBA texture input and region coordinates.
- `generation-prompt.txt`: the exact prompt from the texture-repainting step. Its named image paths are historical authoring labels; the offline animation build uses the four pinned inputs above.

`../polish_animation.py` reads these files and writes the final native assets into `data/anim/foundry_warden/`. Keep this source texture unchanged; heel/toe, emissive and rear-leg regions are derived by the script. The image-generation service is not required for rebuilding the rig or preview.

Provenance and CC BY-SA 3.0 attribution: [ATTRIBUTION.md](../ATTRIBUTION.md).
