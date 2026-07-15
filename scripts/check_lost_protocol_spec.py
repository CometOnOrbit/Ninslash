#!/usr/bin/env python3
"""Strict static acceptance checks for Lost Protocol gameplay contracts.

This script intentionally checks semantics that are easy to lose during refactors. It
does not generate files or modify the source tree.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ERRORS: list[str] = []


def read(relative: str) -> str:
    try:
        return (ROOT / relative).read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        ERRORS.append(f"cannot read {relative}: {error}")
        return ""


def expect(condition: bool, message: str) -> None:
    if not condition:
        ERRORS.append(message)


def tokens(source: str, required: tuple[str, ...], scope: str) -> None:
    for token in required:
        expect(token in source, f"{scope}: missing marker {token!r}")


def check_operations() -> None:
    source = read("src/game/pve_roguelite.cpp")
    rows = re.findall(r"^\s*\{(PVE_OPERATION_[A-Z0-9_]+),.*?\},\s*$", source, re.MULTILINE)
    expected = {
        "PVE_OPERATION_CIRCUIT_BREAKER": ("PVE_MODE_INVASION", ("SHIELD_RELAY", "OVERLOAD_TERMINAL", "BOSS"), (2, 40, 1)),
        "PVE_OPERATION_FOUNDRY_SHUTDOWN": ("PVE_MODE_INVASION", ("ASSEMBLY_NODE", "COOLANT_CORE", "EVACUATION"), (3, 1, 60)),
        "PVE_OPERATION_FIRE_CONTROL_PURGE": ("PVE_MODE_INVASION", ("TARGETING_BEACON", "NONE", "BOSS"), (3, 1, 1)),
        "PVE_OPERATION_SIEGE_LINE": ("PVE_MODE_HORDE", ("DEFENSE_AREA", "DEFENSE_AREA", "BOSS"), (1, 2, 1)),
        "PVE_OPERATION_ASSEMBLY_SURGE": ("PVE_MODE_HORDE", ("DEFENSE_AREA", "ASSEMBLY_NODE", "BOSS"), (1, 2, 1)),
        "PVE_OPERATION_GRID_STORM": ("PVE_MODE_HORDE", ("SHIELD_RELAY", "SHIELD_RELAY", "DEFENSE_AREA"), (1, 2, 1)),
        "PVE_OPERATION_CORE_RECOVERY": ("PVE_MODE_EXTRACTION", ("DATA_CORE", "UPLOAD_POINT", "EVACUATION"), (2, 35, 1)),
        "PVE_OPERATION_LOCKDOWN_BREAK": ("PVE_MODE_EXTRACTION", ("SHIELD_NODE", "BOSS", "EVACUATION"), (3, 1, 1)),
        "PVE_OPERATION_SIEGE_ROUTE": ("PVE_MODE_EXTRACTION", ("TARGETING_BEACON", "BOSS", "ENERGY_CORE"), (3, 1, 1)),
    }
    expect(set(rows) == set(expected), f"operations: expected exactly nine IDs, got {sorted(set(rows))}")
    for operation, (mode, target_names, counts) in expected.items():
        match = re.search(rf"^\s*\{{{operation},(.+?)\}},\s*$", source, re.MULTILINE)
        if not match:
            continue
        row = match.group(1)
        expect(mode in row, f"{operation}: wrong or missing mode {mode}")
        targets = tuple(re.findall(r"PVE_OPERATION_TARGET_([A-Z0-9_]+)", row))
        expect(targets == target_names, f"{operation}: targets {targets}, expected {target_names}")
        count_match = re.search(r"\},\s*\{(\d+),\s*(\d+),\s*(\d+)\}\s*$", row)
        actual_counts = tuple(map(int, count_match.groups())) if count_match else ()
        expect(actual_counts == counts, f"{operation}: step requirements {actual_counts}, expected {counts}")


def check_operation_mechanics() -> None:
    source = read("src/game/server/pve_operation_director.cpp")
    hazard = read("src/game/server/entities/pve_operation_hazard.h") + read("src/game/server/entities/pve_operation_hazard.cpp")
    # These named markers may be methods, enums, tags, or comments, but each mechanic
    # must have a distinct implementation path rather than a generic wave counter.
    mechanic_groups = {
        "bombardment": ("BOMBARDMENT", "PVE_OPERATION_FIRE_CONTROL_PURGE"),
        "rotating EMP field": ("ROTATING_EMP", "PVE_OPERATION_GRID_STORM"),
        "shielded reinforcements": ("ShieldPressure", "PVE_OPERATION_LOCKDOWN_BREAK"),
        "high-pressure extraction": ("CorePressure", "PVE_OPERATION_CORE_RECOVERY"),
        "elite wave": ("mixed elite force", "PVE_OPERATION_FIRE_CONTROL_PURGE"),
    }
    for name, required in mechanic_groups.items():
        tokens(source + hazard, required, f"operation mechanic {name}")
    shared = read("src/game/pve_roguelite.cpp")
    tokens(shared, ("PVE_OPERATION_TARGET_SHIELD_RELAY", "PVE_OPERATION_TARGET_ASSEMBLY_NODE", "PVE_OPERATION_TARGET_TARGETING_BEACON"), "distinct relay/node/beacon handling")

    # A generic EVENT_BOSS must not be sufficient: completion must compare the dead
    # entity (or stable entity ID) with the Boss spawned for this operation.
    header = read("src/game/server/pve_operation_director.h")
    identity_field = re.search(r"m_pStageBoss|m_BossEntityID|m_OwnedBoss", header, re.IGNORECASE)
    identity_event = "OwnedEntityAlive(m_pStageBoss" in source
    expect(bool(identity_field), "Boss identity binding: director has no tracked Boss pointer/entity ID")
    expect(bool(identity_event), "Boss identity binding: completion does not verify the spawned Boss identity")

    # Cleanup must only destroy entities spawned by this director, never every droid
    # in the globally appended type range.
    owned_collection = "m_apOwnedEntities" in header and "DestroyOwnedEntities" in source
    expect(bool(owned_collection), "ownership cleanup: no operation-owned entity collection/tag")
    global_range_delete = "m_Type >= DROIDTYPE_BULWARK" in source and "m_Type <= DROIDTYPE_OVERSEER_CORE" in source
    expect(not global_range_delete, "ownership cleanup: Clear() globally deletes all specialist droids")


def check_threat_points() -> None:
    sources = "\n".join(read(path) for path in (
        "src/game/server/bosspool.cpp",
        "src/game/server/bosspool.h",
        "src/game/server/pve_operation_director.cpp",
        "src/game/server/pve_operation_director.h",
        "src/game/server/entities/droid_specialist.cpp",
        "src/game/server/entities/droid_specialist.h",
    ))
    expect(bool(re.search(r"Threat(?:Cost|Points?|Budget)", sources, re.IGNORECASE)), "threat budget: no explicit threat cost/budget API")
    cost_groups = (
        (("RAILGUNNER", "SABOTEUR"), 2),
        (("BULWARK", "ASSEMBLER"), 3),
        (("SIEGE_ENGINE", "OVERSEER_CORE"), 10),
    )
    for droids, points in cost_groups:
        start = sources.find(f"case DROIDTYPE_{droids[0]}")
        end = sources.find(f"return {points};", start)
        block = sources[start:end] if start >= 0 and end >= 0 else ""
        for droid in droids:
            expect(f"DROIDTYPE_{droid}" in block, f"threat budget: {droid} is not explicitly assigned {points} points")
    expect(bool(re.search(r"(?:replace|substitut|consume).*(?:ordinary|normal|batch|budget)", sources, re.IGNORECASE)), "threat budget: no ordinary-spawn replacement/consumption marker")


def check_drone() -> None:
    network = read("datasrc/network.py")
    shared = read("src/game/pve_roguelite.h")
    entity = read("src/game/server/entities/pve_drone.cpp")
    director = read("src/game/server/pve_director.cpp")
    tokens(network, ('NetObject("PveDrone"', "m_Owner", "m_VelX", "m_VelY", "m_Module", "m_State", "m_Health", "m_TargetX", "m_TargetY", "m_ActionTick", "m_SwitchReadyTick"), "drone snapshot")
    for state in ("DEPLOYING", "FOLLOWING", "ACTING", "DISABLED", "DESTROYED", "REBUILDING", "SWITCHING"):
        expect(f"PVE_DRONE_STATE_{state}" in shared, f"drone state missing: {state}")
    tokens(entity, ("m_Health(40)", "TickSpeed() * 12", "ApplyEmp", "CNetObj_PveDrone"), "drone lifecycle")
    expect("CNetObj_Laser" not in entity, "drone snapshot still masquerades as laser")
    tokens(director, ("PVE_DRONE_ASSAULT", "PVE_DRONE_GUARDIAN", "PVE_DRONE_REPAIR", "TickSpeed() * 8"), "drone modules/cooldown")


def check_utf8_layout() -> None:
    source = read("src/engine/client/text.cpp")
    tokens(source, ("str_utf8_decode", "IsCjk", "IsOpeningPunctuation", "IsClosingPunctuation", "IsBreakSpace", "CanBreakBetween", "TextRunLength", "Utf8BytesForCharacters"), "UTF-8 layout")
    expect("Left == '\\n'" in source, "UTF-8 layout: explicit newline is not a forced break")
    expect("Chr == '\\t'" in source, "UTF-8 layout: tab is not a break-space")
    expect("IsOpeningPunctuation(Left) || IsClosingPunctuation(Right)" in source, "UTF-8 layout: forbidden line-edge punctuation rule missing")
    expect("TEXTFLAG_STOP_AT_END" in source, "UTF-8 layout: STOP_AT_END handling missing")
    expect(bool(re.search(r"m_LineWidth\s*>\s*0", source)), "UTF-8 layout: negative/unlimited line width contract missing")
    expect("Compare.m_LineWidth = -1" in source, "UTF-8 layout: wrapping measurement does not use unlimited-width run measurement")


def main() -> int:
    check_operations()
    check_operation_mechanics()
    check_threat_points()
    check_drone()
    check_utf8_layout()
    if ERRORS:
        print(f"ERROR: Lost Protocol strict check found {len(ERRORS)} issue(s):", file=sys.stderr)
        for error in ERRORS:
            print(f" - {error}", file=sys.stderr)
        return 1
    print("OK: nine operations, bespoke mechanics, Boss ownership, threat points, drone states and UTF-8 wrapping")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
