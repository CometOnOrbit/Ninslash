#!/usr/bin/env python3
"""Render SteamPipe templates without storing partner credentials in git."""

import argparse
from pathlib import Path


DEFAULTS = {
    "APP_ID": "1812700",
    "PLAYTEST_APP_ID": "1812730",
    "SERVER_TOOL_APP_ID": "5016790",
    "WINDOWS_CLIENT_DEPOT_ID": "1812702",
    "LINUX_CLIENT_DEPOT_ID": "1812703",
    "MACOS_CLIENT_DEPOT_ID": "1812704",
    "WINDOWS_SERVER_DEPOT_ID": "5016792",
    "LINUX_SERVER_DEPOT_ID": "5016793",
    "MACOS_SERVER_DEPOT_ID": "5016794",
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--build-output", required=True,
                        help="Base SteamPipe output directory; per-app subdirs are created")
    parser.add_argument("--content-root", required=True)
    parser.add_argument("--windows-client-root", default="")
    parser.add_argument("--linux-client-root", default="")
    parser.add_argument("--macos-client-root", default="")
    parser.add_argument("--windows-server-root", default="")
    parser.add_argument("--linux-server-root", default="")
    parser.add_argument("--macos-server-root", default="")
    parser.add_argument("--platforms", default="windows,linux,macos",
                        help="Comma-separated Steam depot platforms to render")
    parser.add_argument("--version", default="local")
    parser.add_argument("--git-commit", default="unknown")
    parser.add_argument("--client-set-live", default="")
    parser.add_argument("--playtest-set-live", default="")
    parser.add_argument("--server-set-live", default="")
    parser.add_argument("--playtest-app-id", default=DEFAULTS["PLAYTEST_APP_ID"])
    args = parser.parse_args()

    platforms = {platform.strip().lower() for platform in args.platforms.split(",") if platform.strip()}
    unknown_platforms = platforms - {"windows", "linux", "macos"}
    if not platforms or unknown_platforms:
        parser.error(f"invalid --platforms value: {args.platforms}")
    for platform in platforms:
        for kind in ("client", "server"):
            if not getattr(args, f"{platform}_{kind}_root"):
                parser.error(f"--platforms including {platform} requires --{platform}-{kind}-root")

    build_output = Path(args.build_output)
    values = dict(DEFAULTS)
    values.update({
        "PLAYTEST_APP_ID": args.playtest_app_id,
        # Separate SteamPipe working dirs per app so simultaneous depot IDs
        # (shared playtest) and sequential builds do not clobber each other.
        "CLIENT_BUILD_OUTPUT": str(build_output / "client"),
        "PLAYTEST_BUILD_OUTPUT": str(build_output / "playtest"),
        "SERVER_BUILD_OUTPUT": str(build_output / "server"),
        "CONTENT_ROOT": args.content_root,
        "WINDOWS_CLIENT_ROOT": args.windows_client_root,
        "LINUX_CLIENT_ROOT": args.linux_client_root,
        "MACOS_CLIENT_ROOT": args.macos_client_root,
        "WINDOWS_SERVER_ROOT": args.windows_server_root,
        "LINUX_SERVER_ROOT": args.linux_server_root,
        "MACOS_SERVER_ROOT": args.macos_server_root,
        "VERSION": args.version,
        "GIT_COMMIT": args.git_commit,
        "CLIENT_SET_LIVE": args.client_set_live,
        "PLAYTEST_SET_LIVE": args.playtest_set_live,
        "SERVER_SET_LIVE": args.server_set_live,
    })
    for path in (
        values["CLIENT_BUILD_OUTPUT"],
        values["PLAYTEST_BUILD_OUTPUT"],
        values["SERVER_BUILD_OUTPUT"],
    ):
        Path(path).mkdir(parents=True, exist_ok=True)

    root = Path(__file__).resolve().parents[1] / "packaging" / "steam"
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    for stale_manifest in output.glob("*.vdf"):
        stale_manifest.unlink()
    for source in root.glob("*.vdf.in"):
        depot_platform = next((platform for platform in ("windows", "linux", "macos")
                               if source.name.startswith(f"depot_{platform}_")), None)
        if depot_platform and depot_platform not in platforms:
            continue
        rendered = source.read_text(encoding="utf-8")
        for platform in {"windows", "linux", "macos"} - platforms:
            marker = f"@{platform.upper()}_"
            rendered = "\n".join(line for line in rendered.splitlines() if marker not in line) + "\n"
        for key, value in values.items():
            rendered = rendered.replace(f"@{key}@", value)
        # No setlive entry means "create the Build without changing a branch".
        # Keep upload permission separate from the stronger publish action.
        rendered = "\n".join(
            line for line in rendered.splitlines()
            if not (line.strip().startswith('"setlive"') and line.strip().endswith('""'))
        ) + "\n"
        if "@" in rendered:
            raise RuntimeError(f"unresolved template token in {source.name}")
        (output / source.name.removesuffix(".in")).write_text(rendered, encoding="utf-8")


if __name__ == "__main__":
    main()
