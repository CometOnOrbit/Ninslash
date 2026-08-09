#!/usr/bin/env bash
set -Eeuo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)/.."
cd "$ROOT"

SDK_ROOT="${STEAMWORKS_SDK_ROOT:-$HOME/sdk}"
LINUX_DIR="${LINUX_BUILD_DIR:-build-steam-linux}"
WINDOWS_DIR="${WINDOWS_BUILD_DIR:-build-windows-steam}"
OUT="${OUTPUT_ROOT:-dist/steam-local-test}"
PLATFORMS="${PLATFORMS:-linux,windows}"
UPLOAD="${UPLOAD:-0}"
NO_BUILD="${NO_BUILD:-0}"
UPLOAD_TARGET="${UPLOAD_TARGET:-all}"
SET_LIVE="${STEAM_SET_LIVE_BRANCH:-}"

args=(
  --linux-build-dir "$LINUX_DIR"
  --windows-build-dir "$WINDOWS_DIR"
  --platforms "$PLATFORMS"
  --sdk-root "$SDK_ROOT"
  --output-root "$OUT"
  --strict-assets
  --jobs "${JOBS:-$(nproc)}"
)

if [[ -d dist/steam-macos/macos-client && -d dist/steam-macos/macos-server ]]; then
  args+=(
    --macos-client-depot dist/steam-macos/macos-client
    --macos-server-depot dist/steam-macos/macos-server
    --platforms windows,linux,macos
  )
fi

if [[ "$NO_BUILD" == 1 ]]; then
  args+=(--no-build)
fi

if [[ "$UPLOAD" == 1 ]]; then
  test -n "${STEAM_ACCOUNT:-}" || {
    echo "Set STEAM_ACCOUNT to your Steam Partner build account" >&2
    exit 2
  }
  if [[ "$UPLOAD_TARGET" == playtest ]]; then
    echo "Playtest AppID 1812730 cannot be uploaded via SteamPipe with shared depots." >&2
    echo "Use UPLOAD_TARGET=client, then promote Playtest internal in Steamworks." >&2
    exit 2
  fi
  args+=(--upload --steam-account "$STEAM_ACCOUNT" --upload-target "$UPLOAD_TARGET")
  if [[ -n "$SET_LIVE" ]]; then
    args+=(--set-live "$SET_LIVE")
    echo "Will set live branch: $SET_LIVE"
  else
    echo "STEAM_SET_LIVE_BRANCH unset; uploading builds only (promote in Steamworks manually)"
  fi
fi

echo "Manifest output: $OUT/manifests"
exec python3 scripts/publish_steam_depots.py "${args[@]}" "$@"
