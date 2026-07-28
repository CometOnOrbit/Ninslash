# Steamworks release upload checklist

- Upload all `rich_presence_*.vdf` files under **Installation > General > Rich Presence Localization** and publish the changes.
- Register the `#Status_*` display tokens and confirm English, Simplified Chinese, and Traditional Chinese previews.
- Configure Workshop tags: `Maps`, `Room Presets`, `Challenges`, `Mods`, `Gameplay`, `Weapons`, `Items`, `Resources`, `everyone`, `teen`, `mature`.
- Upload Timeline icons used by the client, or retain the built-in `steam_*` icon names.
- Set `data/steam_input_manifest.vdf` as the Steam Input manifest, then publish official Xbox, PlayStation, Switch Pro, Steam Deck, and Remote Play Touch layouts for every action set.
- Verify the default screenshot hotkey remains owned by the Steam overlay. The game must not enable `HookScreenshots`.
- Test Workshop legal-agreement flow, item visibility, dependencies, preview images, and update progress with a non-owner account.
- Test Rich Presence joins only expose validated `connect` targets and use the party Lobby as `steam_player_group`.
- Test Timeline/Game Recording and community challenge leaderboards with two Steam accounts. Community boards must be labelled “Unverified”.
