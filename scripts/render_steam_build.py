#!/usr/bin/env python3
"""Render SteamPipe templates without storing partner credentials in git."""

import argparse
from pathlib import Path


DEFAULTS = {
    "APP_ID": "1812700",
    "SERVER_TOOL_APP_ID": "5016790",
    "WINDOWS_CLIENT_DEPOT_ID": "1812702",
    "LINUX_CLIENT_DEPOT_ID": "1812703",
    "WINDOWS_SERVER_DEPOT_ID": "5016792",
    "LINUX_SERVER_DEPOT_ID": "5016793",
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True)
    parser.add_argument("--build-output", required=True)
    parser.add_argument("--content-root", required=True)
    parser.add_argument("--windows-client-root", required=True)
    parser.add_argument("--linux-client-root", required=True)
    parser.add_argument("--windows-server-root", required=True)
    parser.add_argument("--linux-server-root", required=True)
    parser.add_argument("--version", default="local")
    parser.add_argument("--git-commit", default="unknown")
    parser.add_argument("--client-set-live", default="")
    parser.add_argument("--server-set-live", default="")
    args = parser.parse_args()

    values = dict(DEFAULTS)
    values.update({
        "BUILD_OUTPUT": args.build_output,
        "CONTENT_ROOT": args.content_root,
        "WINDOWS_CLIENT_ROOT": args.windows_client_root,
        "LINUX_CLIENT_ROOT": args.linux_client_root,
        "WINDOWS_SERVER_ROOT": args.windows_server_root,
        "LINUX_SERVER_ROOT": args.linux_server_root,
        "VERSION": args.version,
        "GIT_COMMIT": args.git_commit,
        "CLIENT_SET_LIVE": args.client_set_live,
        "SERVER_SET_LIVE": args.server_set_live,
    })
    root = Path(__file__).resolve().parents[1] / "packaging" / "steam"
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    for source in root.glob("*.vdf.in"):
        rendered = source.read_text(encoding="utf-8")
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
