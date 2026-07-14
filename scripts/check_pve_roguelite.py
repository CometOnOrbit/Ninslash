#!/usr/bin/env python3
"""Static invariants for the shared co-op PvE Roguelite definitions."""

from __future__ import annotations

import csv
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..")


def fail(message: str) -> None:
    raise AssertionError(message)


def parse_cards(source: str) -> list[tuple[str, list[str]]]:
    result: list[tuple[str, list[str]]] = []
    for match in re.finditer(r"^\s*(CARD[013])\((.*)\),$", source, re.MULTILINE):
        result.append((match.group(1), next(csv.reader([match.group(2)], skipinitialspace=True))))
    return result


def main() -> int:
    shared_path = os.path.join(ROOT, "src/game/pve_roguelite.cpp")
    shared = open(shared_path, encoding="utf-8").read()
    cards = parse_cards(shared)
    if len(cards) != 100:
        fail(f"expected 100 cards, found {len(cards)}")
    symbols = [row[0] for _, row in cards]
    names = [row[1] for _, row in cards]
    if len(set(symbols)) != 100 or len(set(names)) != 100:
        fail("card IDs and display names must be unique")
    if symbols[39] != "PVE_CARD_NO_ONE_LEFT" or symbols[40] != "PVE_CARD_MARKING_ROUNDS" or symbols[52] != "PVE_CARD_PREDATOR":
        fail("legacy or extension card ID boundary changed")

    symbol_index = {symbol: i for i, symbol in enumerate(symbols)}
    graph: dict[str, list[str]] = {}
    base_count = new_base_count = legendary_count = 0
    new_research_cost = 0
    for i, (macro, row) in enumerate(cards):
        symbol, rarity = row[0], row[3]
        stacks, cost = int(row[4]), int(row[5])
        base, legendary = row[6] == "true", row[7] == "true"
        if not 1 <= stacks <= 3:
            fail(f"invalid stack count for {symbol}")
        if rarity != "PVE_RARITY_COMMON" and stacks != 1:
            fail(f"non-common card {symbol} must be unique")
        if legendary != (rarity == "PVE_RARITY_LEGENDARY"):
            fail(f"legendary flag mismatch for {symbol}")
        prereqs = [] if macro == "CARD0" else ([row[9]] if macro == "CARD1" else row[9:12])
        graph[symbol] = prereqs
        for prereq in prereqs:
            if prereq not in symbol_index:
                fail(f"missing prerequisite {prereq}")
        if base and (cost != 0 or prereqs):
            fail(f"base card {symbol} has research metadata")
        base_count += base
        new_base_count += base and 40 <= i <= 51
        legendary_count += legendary
        if i >= 52:
            new_research_cost += cost
    if (base_count, new_base_count, legendary_count) != (19, 12, 8):
        fail(f"content totals mismatch: base={base_count}, new base={new_base_count}, legendary={legendary_count}")
    if not 150 <= new_research_cost <= 165:
        fail(f"new research cost should be about 155, got {new_research_cost}")

    state: dict[str, int] = {}
    def visit(symbol: str) -> None:
        if state.get(symbol) == 1:
            fail(f"cyclic prerequisite at {symbol}")
        if state.get(symbol) == 2:
            return
        state[symbol] = 1
        for prereq in graph[symbol]:
            visit(prereq)
        state[symbol] = 2
    for symbol in symbols:
        visit(symbol)

    contracts = re.findall(
        r'^\s*\{(PVE_CONTRACT_[A-Z0-9_]+),\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)",\s*(PVE_MODE_[A-Z]+)\},',
        shared, re.MULTILINE,
    )
    if len(contracts) != 20 or len({contract[0] for contract in contracts}) != 20:
        fail(f"expected 20 unique contracts, found {len(contracts)}")
    if contracts[11][0] != "PVE_CONTRACT_OVERRUN" or contracts[12][0] != "PVE_CONTRACT_ATTRITION":
        fail("legacy contract IDs changed")

    network = open(os.path.join(ROOT, "datasrc/network.py"), encoding="utf-8").read()
    for message in (
        "Sv_PveProgress", "Sv_PveChoice", "Sv_PvePerk", "Sv_PveContractVote",
        "Sv_PveContractStatus", "Sv_PveResearchReward", "Sv_PveValidation",
        "Sv_PveBuildState", "Cl_PveProgress", "Cl_PveChoice", "Cl_PveContractVote",
        "Cl_PveResearchBuy", "Cl_PveDroneModule",
    ):
        if f'NetMessage("{message}"' not in network:
            fail(f"missing protocol message {message}")
    for field in ("m_ResearchMask0", "m_ResearchMask1", "m_ResearchMask2", "m_ResearchMask3"):
        if network.count(field) != 2:
            fail(f"128-bit progress field {field} missing from one direction")
    for boundary in (
        'NetIntRange("m_Card0", 0, 102)', 'NetIntRange("m_Contract0", 0, 19)',
        'NetIntRange("m_Card", 7, 99)', 'NetIntRange("m_Module", 1, 3)',
    ):
        if boundary not in network:
            fail(f"protocol boundary missing: {boundary}")

    director = open(os.path.join(ROOT, "src/game/server/pve_director.cpp"), encoding="utf-8").read()
    client = open(os.path.join(ROOT, "src/game/client/components/pve_roguelite.cpp"), encoding="utf-8").read()
    invasion = open(os.path.join(ROOT, "src/game/server/gamemodes/invasion.cpp"), encoding="utf-8").read()
    building = open(os.path.join(ROOT, "src/game/server/entities/building.cpp"), encoding="utf-8").read()
    # Every extension card and contract must have a server-side integration
    # point outside the data table. This prevents definition-only content from
    # silently entering the draw pool.
    server_sources = ""
    for dirpath, _, filenames in os.walk(os.path.join(ROOT, "src/game/server")):
        for filename in filenames:
            if filename.endswith((".cpp", ".h")):
                server_sources += open(os.path.join(dirpath, filename), encoding="utf-8", errors="replace").read()
    for symbol in symbols[40:]:
        if symbol not in server_sources:
            fail(f"new card has no server integration: {symbol}")
    for symbol, *_ in contracts[12:]:
        if symbol not in server_sources:
            fail(f"new contract has no server integration: {symbol}")
    required_server = (
        "const int aRarityWeights[4] = {55, 30, 10, 5}",
        "Run.m_ResearchMask.Set(CardID)", "Run.m_ResearchMask.PrerequisitesMet(CardID)",
        "Run.m_LegendaryCard < 0", "m_DroneSwitchReadyTick", "TickTargetStatuses()",
        "m_ApplyingSecondaryEffect", "Msg.m_Focus", "AddBarrier(ClientID",
    )
    for token in required_server:
        if token not in director:
            fail(f"server authority invariant missing: {token}")
    for token in ("Length != 16 && Length != 32", '"%016llX%016llX"', "Msg.m_ResearchMask3", "NETMSGTYPE_SV_PVEBUILDSTATE"):
        if token not in client:
            fail(f"client migration/state invariant missing: {token}")
    for token in (
        "pSwitch->SetPveSwitchActive(false)",
        "m_Quest != QUEST_ACTIVATE_SWITCHES && m_Quest != QUEST_FIND_SWITCH",
        "SetSwitchesActive(true)",
    ):
        if token not in invasion:
            fail(f"Invasion switch objective gate missing: {token}")
    for token in (
        "m_Type == BUILDING_SWITCH && !m_PveSwitchActive",
        "m_Collision = Active",
    ):
        if token not in building:
            fail(f"hidden switch authority invariant missing: {token}")

    message_names = re.findall(r'^\s*NetMessage\("([^"]+)"', network, re.MULTILINE)
    legacy_messages = (
        "Sv_Broadcast", "Sv_GameVote", "Sv_GameVoteStatus", "Sv_Chat", "Sv_KillMsg",
        "Sv_SoundGlobal", "Sv_TuneParams", "Sv_ExtraProjectile", "Sv_ReadyToEnter",
        "Sv_WeaponPickup", "Sv_Emoticon", "Sv_VoteClearOptions", "Sv_VoteOptionListAdd",
        "Sv_VoteOptionAdd", "Sv_VoteOptionRemove", "Sv_VoteSet", "Sv_VoteStatus",
        "Sv_Inventory", "Cl_Say", "Cl_SetTeam", "Cl_SetSpectatorMode", "Cl_StartInfo",
        "Cl_ChangeInfo", "Cl_Kill", "Cl_Emoticon", "Cl_DropWeapon", "Cl_SelectItem",
        "Cl_UseKit", "Cl_Vote", "Cl_VoteGameMode", "Cl_CallVote", "Cl_InventoryAction",
    )
    if tuple(message_names[:len(legacy_messages)]) != legacy_messages:
        fail("legacy protocol message IDs changed")

    config = open(os.path.join(ROOT, "src/engine/shared/config_variables.h"), encoding="utf-8").read()
    if 'cl_pve_research_mask, 33, "00000000000000000000000000000000"' not in config:
        fail("research mask config is not 128-bit hexadecimal")
    version = open(os.path.join(ROOT, "src/game/version.h"), encoding="utf-8").read()
    if '"pve-director-v7"' not in version:
        fail("network protocol version was not advanced to v7")

    print(f"OK: 100 cards (12 new base, 48 new research, 8 legendary), 20 contracts, {new_research_cost} new research points, v7 protocol")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
