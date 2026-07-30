#!/usr/bin/env python3

import contextlib
import io
import sys
import textwrap
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

import publish_steam_depots


def run_case(command, expected):
    password = "not-a-real-steam-password"
    captured = io.BytesIO()

    class Output:
        buffer = captured

        @staticmethod
        def write(value):
            captured.write(value.encode("utf-8"))
            return len(value)

        @staticmethod
        def flush():
            pass

    with contextlib.redirect_stdout(Output()):
        output = publish_steam_depots.run_streamed(command, password=password)

    visible = captured.getvalue().decode("utf-8", errors="replace")
    if password in visible or password in output:
        raise SystemExit("Steam password was exposed in command output")
    if expected not in output:
        raise SystemExit("Steam password prompt was not answered")


def main():
    run_case(
        [
            sys.executable,
            "-c",
            "import getpass; value=getpass.getpass('Password: '); print('accepted' if value else 'empty')",
        ],
        "accepted",
    )
    run_case(
        [
            sys.executable,
            "-c",
            textwrap.dedent("""
                import sys
                import time

                sys.stdout.write("\\x1b[33mpass")
                sys.stdout.flush()
                time.sleep(0.05)
                sys.stdout.write("word:\\x1b[0m ")
                sys.stdout.flush()
                value = sys.stdin.readline().strip()
                print("ansi-accepted" if value else "empty")
            """),
        ],
        "ansi-accepted",
    )


if __name__ == "__main__":
    main()
