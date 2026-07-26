# SteamPipe templates

Ninslash uses Steam AppID `1812700`. Copy these templates to a private
Steamworks build workspace and replace the remaining `@...@` tokens with the
DepotIDs, absolute content roots, and output path assigned to the partner
account. Do not commit Steam credentials, cached login tokens, or partner-only
SDK files.

For local SDK testing only, copy `steam_appid.txt.example` next to the built
client and rename it to `steam_appid.txt`. The staging script rejects that file
if it is ever placed in depot content.

Stage content before upload:

```sh
python3 scripts/stage_steam_build.py --platform windows --kind client \
  --build-dir build --output dist/steam/windows-client \
  --steam-api /path/to/steam_api64.dll
python3 scripts/stage_steam_build.py --platform linux --kind server \
  --build-dir build --output dist/steam/linux-server
```

Use separate `internal`, `beta`, and `public` branches. Upload to `internal`,
install through Steam on a clean machine, run the release test matrix, and only
then promote the same BuildID; do not rebuild independently for promotion.
