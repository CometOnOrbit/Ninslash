#!/usr/bin/env python3
"""Behavioral integration gates for the Lost Protocol PvE expansion."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parent.parent


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def main() -> None:
    network = read("datasrc/network.py")
    expected_droids = (
        '"BULWARK", "ASSEMBLER", "SABOTEUR", "RAILGUNNER", '
        '"SIEGE_ENGINE", "OVERSEER_CORE"'
    )
    require(expected_droids in network, "mechanical droid IDs are not appended in v10 order")
    require('NetObject("PveDrone"' in network, "dedicated drone snapshot missing")
    for field in ("m_Step", "m_Progress", "m_Target", "m_TargetType", "m_TargetX", "m_TargetY"):
        require(field in network, f"operation state field missing: {field}")

    shared = read("src/game/pve_roguelite.cpp")
    operation_lines = re.findall(r"^\s*\{PVE_OPERATION_[^\n]+$", shared, re.MULTILINE)
    require(len(operation_lines) == 9, "expected nine operation definitions")
    for line in operation_lines:
        require(len(re.findall(r'"[^"]+"', line)) == 5, "operation must have name, summary and three steps")
        require(len(re.findall(r"PVE_OPERATION_TARGET_[A-Z0-9_]+", line)) == 3, "operation target triplet missing")

    specialists = {
        "droid_bulwark.cpp": ("DROIDTYPE_ASSEMBLER", "DROIDTYPE_RAILGUNNER"),
        "droid_assembler.cpp": ("Repair", "6"),
        "droid_saboteur.cpp": ("TickSpeed() * 5", "ApplyEmp"),
        "droid_railgunner.cpp": ("IntersectLine", "TickSpeed()"),
        "droid_siege_engine.cpp": ("OnHealthThreshold", "DROIDTYPE_BULWARK"),
        "droid_overseer_core.cpp": ("OnHealthThreshold", "DROIDTYPE_ASSEMBLER"),
    }
    for filename, tokens in specialists.items():
        source = read(f"src/game/server/entities/{filename}")
        for token in tokens:
            require(token in source, f"{filename} behavior missing: {token}")
    droid_damage = read("src/game/server/entities/droid.cpp")
    require("300.0f" in droid_damage and "Dmg * 65" in droid_damage, "Bulwark 300-range 35% aura missing")
    specialist_base = read("src/game/server/entities/droid_specialist.cpp")
    for threshold in ("ConsumeThreshold(70", "ConsumeThreshold(35", "ConsumeThreshold(75", "ConsumeThreshold(40"):
        require(threshold in specialist_base, f"Boss phase threshold missing: {threshold}")

    operation = read("src/game/server/pve_operation_director.cpp")
    for token in (
        "PVE_OPERATION_FOUNDRY_SHUTDOWN && m_Stage == 2",
        "EVENT_BOSS", "ClearRisingAcid", "mode-objective-flow",
        "OnOperationChainFinished",
    ):
        require(token in operation, f"operation lifecycle gate missing: {token}")

    drone = read("src/game/server/entities/pve_drone.cpp")
    require("CNetObj_PveDrone" in drone and "CNetObj_Laser" not in drone, "drone still uses laser snapshot")
    require("TickSpeed() * 12" in drone and "m_Health = 40" in drone, "drone rebuild/health contract missing")

    client = read("src/game/client/components/pve_roguelite.cpp")
    require("pDef->m_apSteps[Step]" in client, "operation vote lacks three-step preview")
    require("m_OperationTargetPos - LocalPos" in client, "operation direction HUD missing")

    text = read("src/engine/client/text.cpp")
    for token in ("IsCjk", "IsOpeningPunctuation", "IsClosingPunctuation"):
        require(token in text, f"UTF-8 layout helper missing: {token}")

    for mode in ("invasion", "horde", "extract"):
        cfg = read(f"cfg/{mode}_foundry.cfg")
        require("sv_map generate_foundry1" in cfg, f"{mode} foundry config missing")
        require("sv_mapgen_random_seed 0" in cfg and "sv_mapgen_seed 1337" in cfg, f"{mode} fixed seed missing")
    require((ROOT / "data/maps/generate_foundry1.map").is_file(), "foundry map missing")

    print("OK: Lost Protocol v10 behavior, drone, operations, UTF-8 layout and foundry integration")


if __name__ == "__main__":
    try:
        main()
    except AssertionError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
