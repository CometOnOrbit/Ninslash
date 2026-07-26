# SteamPipe templates

Ninslash uses AppID `1812700`; Ninslash Dedicated Server uses Tool AppID
`5016790`. The assigned depots are Windows/Linux client `1812702`/`1812703`
and Windows/Linux dedicated server `5016792`/`5016793`. Do not commit Steam
credentials, cached login tokens, or partner-only SDK files.

Render upload-ready templates with the tracked IDs and private absolute paths:

```sh
python3 scripts/render_steam_build.py --output dist/steampipe \
  --build-output /private/steam-output --content-root /private/content \
  --windows-client-root dist/steam/windows-client \
  --linux-client-root dist/steam/linux-client \
  --windows-server-root dist/steam/windows-server \
  --linux-server-root dist/steam/linux-server
```

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
