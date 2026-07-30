# Plasma Carbine example Mod

This is a complete, directly installable Lua weapon Mod. It demonstrates:

- inheritance from an official weapon;
- a custom frame, compatible combinations, and a forge recipe;
- calculated level curves instead of 16 copied values;
- real in-game PNG art cropped from Ninslash's built-in atlases, plus a custom
  WV firing sound;
- combat callbacks and integer state slots;
- a separate, read-only client HUD callback;
- Simplified and Traditional Chinese localization overlays.

## Install

In **Local Mods**, press **Import ZIP** and choose the packaged example. For a
manual installation, copy this directory to the game's save directory as:

```text
workshop/plasma-carbine-example/
```

Select **Plasma Carbine Example** and press **Enable Mod**. No console variable
needs to be edited. The directory name is only a local label; package identity
comes from `published_file_id` in `ninslash_content.json`.

## Test the weapon

The full stable ID is:

```text
workshop:9000000001:plasma-carbine
```

On a development server, spawn level 0 beside client 0 with:

```text
weapon_spawn 0 workshop:9000000001:plasma-carbine 0
```

Firing cycles an integer charge state from 0 through 3. The runtime callback
uses that state to increase projectile damage, while the presentation callback
draws the same weapon state as a HUD bar.

## Files

- `plasma.weapon.lua`: definition, balance curves, visuals, assets, localization.
- `plasma_variant.weapon.lua`: a second definition listed first in the manifest to demonstrate canonical path-ordered loading.
- `plasma.weapon_runtime.lua`: custom `on_fire` behavior.
- `plasma.weapon_presentation.lua`: client-only HUD behavior.
- `resources/`: held weapon, projectile, muzzle, and firing sound assets.
- `localization/`: translations for the definition's source keys.

After changing any declared file or manifest field, update the package hash from
the repository root:

```sh
python3 scripts/content_hash.py examples/workshop_weapon --write
```

Runtime callbacks use the same restricted API as official weapons. They cannot
override another package's IDs or access engine pointers, files, sockets,
dynamic loading, or Steam APIs.
