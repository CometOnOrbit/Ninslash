#!/usr/bin/env python3
"""Verify server JSON and client .txt language files cover user-facing strings."""

from __future__ import annotations

import json
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..")


def extract_quest_strings(function_names: tuple[str, ...]) -> set[str]:
    quest_path = os.path.join(ROOT, "src/game/questinfo.cpp")
    with open(quest_path, encoding="utf-8") as f:
        text = f.read()

    strings: set[str] = set()
    for name in function_names:
        marker = f"const char *{name}("
        start = text.find(marker)
        if start < 0:
            continue
        end = text.find("\nconst char *", start + len(marker))
        body = text[start : len(text) if end < 0 else end]
        strings.update(m.group(1) for m in re.finditer(r'return "([^"]+)"', body) if m.group(1))
    return strings


def extract_server_strings() -> set[str]:
    strings = extract_quest_strings(("GetQuestStartMessage", "GetQuestCompletedMessage"))

    server_root = os.path.join(ROOT, "src/game/server")
    patterns = [
        r'SendBroadcast\(\s*"([^"]*)"',
        r'SendBroadcastFormat\([^,]+,\s*[^,]+,\s*"([^"]+)"',
        r'SendChatTarget\([^,]+,\s*"([^"]+)"',
    ]
    for dirpath, _, filenames in os.walk(server_root):
        for fn in filenames:
            if not fn.endswith(".cpp"):
                continue
            text = open(os.path.join(dirpath, fn), encoding="utf-8", errors="replace").read()
            for pat in patterns:
                for m in re.finditer(pat, text):
                    if m.group(1):
                        strings.add(m.group(1))

    gamevotes_root = os.path.join(ROOT, "data/server/gamevotes")
    for fn in os.listdir(gamevotes_root):
        if not fn.endswith(".vot"):
            continue
        with open(os.path.join(gamevotes_root, fn), encoding="utf-8") as f:
            text = f.read()
        strings.update(
            m.group(1)
            for m in re.finditer(r"^(?:name|description): (.+)$", text, re.MULTILINE)
            if m.group(1)
        )

    strings.discard("")
    strings.discard("All players were moved to the %s")
    return strings


def extract_client_strings() -> set[str]:
    strings = extract_quest_strings(
        ("GetQuestDisplayName", "GetThemeDisplayName", "GetWaveDisplayName")
    )
    game_root = os.path.join(ROOT, "src/game")
    for dirpath, _, filenames in os.walk(game_root):
        for fn in filenames:
            if not fn.endswith((".cpp", ".h")):
                continue
            text = open(os.path.join(dirpath, fn), encoding="utf-8", errors="replace").read()
            for m in re.finditer(r'Localize\("([^"]+)"\)', text):
                strings.add(m.group(1))
    return strings


def load_json_keys(path: str) -> set[str]:
    with open(path, encoding="utf-8") as f:
        return {item["key"] for item in json.load(f)["translation"]}


def load_client_keys(path: str) -> set[str]:
    keys: set[str] = set()
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if line and not line.startswith("#") and not line.startswith("==") and not line.startswith("####"):
                keys.add(line)
    return keys


def main() -> int:
    errors = 0

    server_strings = extract_server_strings()
    en_path = os.path.join(ROOT, "data/server/languages/en-template.json")
    en_keys = load_json_keys(en_path)
    missing_en = sorted(server_strings - en_keys)
    if missing_en:
        errors += 1
        print("Missing from en-template.json:")
        for key in missing_en:
            print(f"  {key}")

    for zh_name in ("zh-cn.json", "zh-hk.json"):
        zh_path = os.path.join(ROOT, "data/server/languages", zh_name)
        if not os.path.exists(zh_path):
            errors += 1
            print(f"Missing language file: {zh_name}")
            continue
        zh_keys = load_json_keys(zh_path)
        missing_zh = sorted(server_strings - zh_keys)
        if missing_zh:
            errors += 1
            print(f"Missing from {zh_name}:")
            for key in missing_zh:
                print(f"  {key}")

    client_strings = extract_client_strings()
    lang_dir = os.path.join(ROOT, "data/languages")
    for fn in sorted(os.listdir(lang_dir)):
        if not fn.endswith(".txt") or fn == "index.txt":
            continue
        keys = load_client_keys(os.path.join(lang_dir, fn))
        missing = sorted(client_strings - keys)
        if missing:
            errors += 1
            print(f"Missing from client {fn} ({len(missing)} keys)")

    if errors == 0:
        print(
            f"OK: {len(server_strings)} server strings, "
            f"{len(client_strings)} client Localize() strings"
        )
        return 0

    print(f"\n{errors} localization issue(s) found.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
