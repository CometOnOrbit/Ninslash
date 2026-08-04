#!/usr/bin/env python3
"""Verify server JSON and client .txt language files cover user-facing strings."""

from __future__ import annotations

import ast
import json
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..")


def strip_cpp_comments(text: str) -> str:
    """Remove comments without treating comment markers inside literals as syntax."""
    pattern = re.compile(
        r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*.*?\*/',
        re.DOTALL,
    )

    def replace(match: re.Match[str]) -> str:
        value = match.group(0)
        if value.startswith(('"', "'")):
            return value
        return "\n" * value.count("\n")

    return pattern.sub(replace, text)


def extract_c_string_expressions(text: str) -> set[str]:
    """Extract C string expressions, concatenating adjacent string literals."""
    matches = list(re.finditer(r'"(?:\\.|[^"\\])*"', text))
    strings: set[str] = set()
    current = ""
    previous_end = -1
    for match in matches:
        value = ast.literal_eval(match.group(0))
        if previous_end >= 0 and not text[previous_end : match.start()].strip():
            current += value
        else:
            if current:
                strings.add(current)
            current = value
        previous_end = match.end()
    if current:
        strings.add(current)
    return strings


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


def extract_localize_literals(text: str) -> set[str]:
    """Extract every literal from Localize expressions, including ternaries."""
    text = strip_cpp_comments(text)
    strings: set[str] = set()
    position = 0
    marker = "Localize("
    while True:
        call = text.find(marker, position)
        if call < 0:
            break
        index = call + len(marker)
        argument_start = index
        depth = 1
        in_string = False
        escaped = False
        while index < len(text) and depth:
            char = text[index]
            if in_string:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == '"':
                    in_string = False
            elif char == '"':
                in_string = True
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
            index += 1
        argument = text[argument_start : index - 1]
        strings.update(extract_c_string_expressions(argument))
        position = max(index, call + 1)
    return strings


def extract_localized_string_tables(text: str) -> set[str]:
    """Extract literal arrays whose entries are later passed to Localize."""
    strings: set[str] = set()
    for match in re.finditer(
        r'(?:static\s+)?const\s+char\s+\*(\w+)(?:\s*\[[^\]]*\])+\s*=\s*\{(.*?)\};',
        text,
        re.DOTALL,
    ):
        name, body = match.groups()
        if not re.search(rf"Localize\(\s*{re.escape(name)}\s*\[", text):
            continue
        strings.update(
            value for value in re.findall(r'"([^"]+)"', body) if "\n" not in value
        )
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
            strings.update(extract_localize_literals(text))
            strings.update(extract_localized_string_tables(text))
    return strings


def extract_platform_status_strings() -> set[str]:
    """Extract status keys that reach Localize through async result structs."""
    strings: set[str] = set()
    for relative in (
        "src/engine/client/platform_services.cpp",
        "src/engine/client/client.cpp",
    ):
        text = strip_cpp_comments(open(os.path.join(ROOT, relative), encoding="utf-8").read())
        patterns = (
            r'str_copy\([^,]*(?:m_aErrorKey|m_aLobbyCreateFailure)[^,]*,\s*"([^"]+)"',
            r'str_copy\([^,]*m_CommunityResult\.m_aError[^,]*,\s*"([^"]+)"',
        )
        for pattern in patterns:
            strings.update(re.findall(pattern, text))
        for call in re.findall(r"SetJoinFailure\((.*?)\);", text, re.DOTALL):
            strings.update(extract_c_string_expressions(call))
    return strings


# These strings are selected through tables or helper return values before they
# reach Localize(), so the direct-call scan above cannot discover them. Keep the
# set close to the menu's user-visible state vocabulary; missing translations
# here otherwise silently fall back to English.
MENU_DYNAMIC_KEYS = {
    "Play", "Character", "Progress", "Workshop", "Replays", "Settings",
    "Continue", "Game", "Change mode", "Players", "Server", "Vote", "Leave",
    "CONTENT", "SYSTEM", "SESSION", "PROFILE",
    "Solo", "Friends", "LAN", "Public", "PVE", "PVP",
    "Choose a game mode", "Configure room", "Step 1 of 2", "Step 2 of 2",
    "Mission difficulty", "AI difficulty", "Target active players",
    "Target players per team", "Target waves", "Mission time",
    "Goal target", "Score limit", "Hide advanced settings", "Advanced settings",
    "LAN binding", "Managed automatically",
    "Restart with changes", "Starting local game", "Stopping local game",
    "Creating room", "Create and join",
    "Continue local mission", "Your local server is running and you are connected.",
    "Join local mission", "Your local server is ready. Rejoin without changing its setup.",
    "Local server starting", "Open server status while the mission is prepared.",
    "Continue training", "Resume chapter %d of 6 from your latest step.",
    "Create a room", "Choose a mode, configure the mission and invite your squad.",
    "Training · complete", "Training · skipped", "Training · not started",
    "Local server · running", "Local server · starting",
    "Local server · attention", "Local server · idle",
    "Steam · online", "Standalone · UDP ready",
    "Training", "Six guided chapters, always replayable.", "SOLO · 30–45 MIN",
    "Choose chapter", "Co-op PvE", "Invasion, Horde, Extraction and Reactor Defense.",
    "4 PVE MODES · CO-OP", "Configure PvE", "Local PvP",
    "Eight competitive modes with adjustable match population.",
    "8 PVP MODES · SOLO / LAN / STEAM", "Choose PvP mode",
    "First Deployment", "Combat and Recovery", "PvE Mission", "Forge and Build",
    "Build and Growth", "Multiplayer Ready",
    "Movement, weapons and the training target.", "Combat, recovery and respawning.",
    "Objectives, defense and extraction.", "Materials, forging and construction.",
    "Perks, drones and research.", "Bot PvP and multiplayer rooms.",
    "Start", "Replay", "Locked", "Resume",
    "Cloud profile is too large", "Steam Cloud conflict needs your choice",
    "Steam Cloud conflict postponed; cloud writes are paused",
    "Steam Cloud data was created by a newer game version",
    "Steam Cloud is synchronizing; retrying...", "Steam Cloud is up to date",
    "Steam Cloud profile applied",
    "Steam Cloud profile is empty; local progress was kept",
    "Steam Cloud profile is invalid; local progress was kept",
    "Steam Cloud upload failed; local progress is safe",
}


def extract_local_game_mode_strings() -> set[str]:
    path = os.path.join(ROOT, "src/game/client/local_game_modes.h")
    text = open(path, encoding="utf-8").read()
    strings: set[str] = {"Target active players", "Target players per team"}
    for line in text.splitlines():
        if not line.lstrip().startswith("LOCAL_MODE_ENTRY(\""):
            continue
        values = re.findall(r'"([^"]*)"', line)
        # Name, description, duration, difficulty and mechanics are localized.
        if len(values) >= 6:
            strings.update((values[0], values[1], values[3], values[4], values[5]))
    for name in (
        "s_apLocalMaps", "s_apLocalCtfMaps", "s_apLocalBallMaps",
        "s_apLocalReactorDefenseMaps", "s_apLocalReactorAssaultMaps",
    ):
        match = re.search(
            rf"static const char \*{name}\[\] = \{{(.*?)\}};", text, re.DOTALL
        )
        if match:
            strings.update(re.findall(r'"([^"]*)"', match.group(1)))
    return strings


def extract_menu_home_strings() -> set[str]:
    path = os.path.join(ROOT, "src/game/client/menu_home.h")
    text = open(path, encoding="utf-8").read()
    strings: set[str] = set()
    for match in re.finditer(
        r'return \{MENU_HOME_[^,]+,\s*"([^"]+)",\s*"([^"]+)"', text
    ):
        strings.update(match.groups())
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
    "Tide: Calm", "Tide: Warning", "Tide: Dark", "Tide: Recovery",
}


def extract_pve_definition_strings() -> set[str]:
    path = os.path.join(ROOT, "src/game/pve_roguelite.cpp")
    text = open(path, encoding="utf-8").read()
    strings: set[str] = set()
    for match in re.finditer(
        r'CARD[013]\(PVE_CARD_[^,]+,\s*"([^"]+)",\s*"([^"]+)"', text
    ):
        strings.update(match.groups())
    short_descriptions = re.search(
        r'gs_apShortCardDescriptions\[NUM_PVE_CARDS\]\s*=\s*\{(.*?)\};',
        text,
        re.DOTALL,
    )
    if short_descriptions:
        strings.update(re.findall(r'"([^"]+)"', short_descriptions.group(1)))
    choice_description = re.search(
        r'const char \*PveChoiceDescription\(int ID\)(.*?)\n\}', text, re.DOTALL
    )
    if choice_description:
        strings.update(re.findall(r'return "([^"]+)";', choice_description.group(1)))
    for match in re.finditer(
        r'\{PVE_CONTRACT_[^,]+,\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)"', text
    ):
        strings.update(match.groups())
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
        print("PvE card/contract display text must not be duplicated in server JSON:")
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
    dynamic_client_strings = (
        set(MENU_DYNAMIC_KEYS)
        | extract_local_game_mode_strings()
        | extract_menu_home_strings()
        | extract_platform_status_strings()
    )
    client_strings = (extract_client_strings() | dynamic_client_strings) - pve_client_strings
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
        invalid_dynamic = sorted(
            key for key in dynamic_client_strings
            if key not in translations
            or not translations[key].strip()
            or "needs translation" in translations[key].lower()
            or placeholders(key) != placeholders(translations[key])
        )
        if invalid_dynamic:
            errors += 1
            print(f"Invalid dynamic menu translations in {fn}: {invalid_dynamic}")
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
