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


def extract_gamevote_strings() -> set[str]:
    strings: set[str] = set()
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


PVE_UI_KEYS = {
    "Research", "Team Contract", "Choose a Perk", "%d seconds remaining",
    "Unknown contract", "RISK • %d VOTES", "%s • LEVEL %d/%d",
    "Immediate Supply", "Reward: 1 Research Point", "Voted", "Selected", "Select",
    "Mouse • Arrow Keys • 1–3 • Gamepad",
    "The server rejected that selection.", "Contract completed", "Contract failed",
    "%d seconds • %d/%d", "Active • %d/%d", "%d Research Points",
    "Core", "Weapons", "Modes", "PURCHASED", "AVAILABLE", "LOCKED",
    "%s • TIER %d • COST %d", "Requires: %s", "No prerequisite",
    "Purchased", "Purchase", "Locked", "Current Run", "Common", "Rare", "Epic",
    "Emergency Armor", "Full Ammunition", "Emergency Kits", "Unknown perk",
    "Gain 25 armor immediately.", "Refill all weapon ammunition immediately.",
    "Gain 5 kits immediately.",
    "Checkpoint %d",
	"+%d more perks", "After selection: level %d/%d", "Immediate effect",
	"Unique perk", "Stack limit: %d",
	"Attack", "Survival", "Logistics", "Firearms", "Explosives", "Electric",
	"Melee", "Invasion", "Horde", "Extraction",
    "Purchase rejected",
	"Legendary", "Barrier", "Focus", "Blast Charge", "Voltage", "Fury",
	"Vulnerable", "Bleed", "Drone", "None", "Assault", "Guardian", "Repair",
	"Drone module not owned", "Drone switch cooling down",
	"Choose an Operation", "Vote for the team's next mission route.",
	"Mouse / Arrow Keys / 1-2 / Gamepad", "ACTIVE OPERATION",
}


def extract_pve_definition_strings() -> set[str]:
    path = os.path.join(ROOT, "src/game/pve_roguelite.cpp")
    text = open(path, encoding="utf-8").read()
    strings: set[str] = set()
    for match in re.finditer(
        r'CARD[013]\(PVE_CARD_[^,]+,\s*"([^"]+)",\s*"([^"]+)"', text
    ):
        strings.update(match.groups())
    for match in re.finditer(
        r'\{PVE_CONTRACT_[^,]+,\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)"', text
    ):
        strings.update(match.groups())
    for match in re.finditer(r'^\s*\{PVE_OPERATION_[^\n]+$', text, re.MULTILINE):
        strings.update(re.findall(r'"([^"]+)"', match.group(0)))
    return strings


def extract_pve_client_strings() -> set[str]:
    return set(PVE_UI_KEYS) | extract_pve_definition_strings()


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


def load_json_map(path: str) -> dict[str, str]:
    with open(path, encoding="utf-8") as f:
        return {item["key"]: item["value"] for item in json.load(f)["translation"]}


def load_client_map(path: str) -> dict[str, str]:
    result: dict[str, str] = {}
    key: str | None = None
    with open(path, encoding="utf-8") as f:
        for raw in f:
            line = raw.rstrip("\n")
            if line.startswith("== ") and key is not None:
                result[key] = line[3:]
                key = None
            elif line and not line.startswith(("#", "==", "####")):
                key = line
    return result


def placeholders(text: str) -> tuple[str, ...]:
    return tuple(re.findall(r"%(?:\d+\$)?[diuoxXfFeEgGaAcsp]", text.replace("%%", "")))


def main() -> int:
    errors = 0

    server_strings = extract_server_strings()
    gamevote_strings = extract_gamevote_strings()
    all_server_strings = server_strings | gamevote_strings
    en_path = os.path.join(ROOT, "data/server/languages/en-template.json")
    en_keys = load_json_keys(en_path)
    definition_overlap = sorted(extract_pve_definition_strings() & en_keys)
    if definition_overlap:
        errors += 1
        print("PvE card/contract/operation display text must not be duplicated in server JSON:")
        for key in definition_overlap:
            print(f"  {key}")
    missing_en = sorted(all_server_strings - en_keys)
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
        missing_zh = sorted(all_server_strings - zh_keys)
        if missing_zh:
            errors += 1
            print(f"Missing from {zh_name}:")
            for key in missing_zh:
                print(f"  {key}")

    pve_client_strings = extract_pve_client_strings()
    client_strings = extract_client_strings() - pve_client_strings
    lang_dir = os.path.join(ROOT, "data/languages")
    for fn in ("simplified_chinese.txt", "traditional_chinese.txt"):
        keys = load_client_keys(os.path.join(lang_dir, fn))
        missing = sorted(client_strings - keys)
        if missing:
            errors += 1
            print(f"Missing from client {fn} ({len(missing)} keys)")

    for fn in ("simplified_chinese.txt", "traditional_chinese.txt"):
        path = os.path.join(lang_dir, fn)
        translations = load_client_map(path)
        missing = sorted(pve_client_strings - translations.keys())
        if missing:
            errors += 1
            print(f"Missing PvE client strings from {fn} ({len(missing)} keys):")
            for key in missing:
                print(f"  {key}")
        invalid = sorted(
            key for key in pve_client_strings & translations.keys()
            if not translations[key].strip()
            or "needs translation" in translations[key].lower()
            or placeholders(key) != placeholders(translations[key])
        )
        if invalid:
            errors += 1
            print(f"Invalid PvE client translations in {fn}: {invalid}")

    pve_server_strings = {
        key for key in server_strings
        if key.startswith(("PvE ", "Team contract", "Contract ", "Perk choice", "Research reward"))
    }
    for fn in ("zh-cn.json", "zh-hk.json"):
        translations = load_json_map(os.path.join(ROOT, "data/server/languages", fn))
        invalid = sorted(
            key for key in pve_server_strings
            if key not in translations
            or not translations[key].strip()
            or translations[key] == key
            or "needs translation" in translations[key].lower()
            or placeholders(key) != placeholders(translations[key])
        )
        if invalid:
            errors += 1
            print(f"Invalid PvE server translations in {fn}: {invalid}")

    if errors == 0:
        print(
			f"OK: {len(server_strings)} server broadcasts, {len(gamevote_strings)} gamevote strings, "
			f"{len(client_strings)} general client strings, {len(pve_client_strings)} PvE client strings"
        )
        return 0

    print(f"\n{errors} localization issue(s) found.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
