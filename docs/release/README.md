# Steam release gates

This directory tracks work that must be complete before a public commercial
distribution, including a free Steam release. Passing a build is not evidence
that the bundled art, audio, fonts, maps, or animations may legally be shipped.

The assigned Ninslash Steam application ID is `1812700`.

## Asset provenance

`asset_licenses.csv` is ordered from broad defaults to specific overrides; the
last matching pattern wins. A row may only be marked `approved` after its
source, author, license, and required attribution have been verified. Files
marked `review_required` or `rejected` block a release build.

Run a progress report:

```sh
python3 scripts/audit_release_assets.py
```

Run the release gate:

```sh
python3 scripts/audit_release_assets.py --strict
```

The current broad `data/**` rule is deliberately unresolved because the
historic project license says some bundled content is nonfree. Do not replace
it with a blanket approval. Add narrow rows for verified files or replace
assets whose provenance cannot be established.

## Player data

The only first-release Steam Cloud file is `pve_progress.json`:

- Windows root: `%APPDATA%/Ninslash`
- Linux root: `$HOME/.ninslash`
- Cloud pattern: `pve_progress.json`
- Cloud settings must exclude `.bak`, `.tmp`, settings, passwords, logs,
  screenshots, demos, videos, and downloaded maps.

`pve_progress.json` is intentionally local-player-authoritative. It must never
contain current-run weapons, gold, inventory, server credentials, IP addresses,
or chat data.

## Remaining non-code gates

- Verify the Ninslash name and logo can be commercially used.
- Produce complete credits and third-party notices from the approved manifest.
- Replace the privacy-policy template with operator legal/contact details.
- Complete the Steam content survey, Early Access questionnaire, age rating,
  support page, store media, and public roadmap.
- Run the strict asset audit before uploading any public depot.
