# Foundry Warden

Invasion stage boss on boss-assault floors (every 10th floor). Spine clips are played by `DROIDTYPE_BOSSWARDEN`.

- `warden-motion-v2.json`: 54 bones, 49 slots and nine animations using region attachments and ordinary translate/rotate/scale tracks.
- `warden-motion-v2.atlas` / `.png`: 26 regions in a 1024×1024 RGBA texture. The atlas page path is relative to the engine's `data/anim/` root.
- `warden-motion-v2.combat.json`: proposed timings and effect sockets, not an engine-executed gameplay configuration.

Authoring inputs, scripts, offline preview, design notes and validation: [design/foundry-warden](../../../design/foundry-warden/README.md).

Derived from Ninslash artwork, distributed under **CC BY-SA 3.0**. Attribution and image-generation provenance: [ATTRIBUTION.md](../../../design/foundry-warden/ATTRIBUTION.md).
