#!/usr/bin/env python3
"""Static invariants for the shared co-op PvE Roguelite definitions."""

from __future__ import annotations

import csv
import os
import re
import subprocess
import sys

ROOT = os.path.join(os.path.dirname(__file__), "..")


def fail(message: str) -> None:
    raise AssertionError(message)


def parse_cards(source: str) -> list[tuple[str, list[str]]]:
    result: list[tuple[str, list[str]]] = []
    for match in re.finditer(r"^\s*(CARD[013])\((.*)\),$", source, re.MULTILINE):
        result.append((match.group(1), next(csv.reader([match.group(2)], skipinitialspace=True))))
    return result


def message_fields(network: str, name: str) -> tuple[str, ...]:
    match = re.search(
        rf'^\s*NetMessage\("{re.escape(name)}", \[\s*(.*?)^\s*\]\),',
        network,
        re.MULTILINE | re.DOTALL,
    )
    if not match:
        fail(f"missing protocol message {name}")
    return tuple(
        re.sub(r"\s+", "", line)
        for line in match.group(1).splitlines()
        if line.strip()
    )


def generate_protocol(mode: str) -> str:
    compile_py = os.path.join(ROOT, "datasrc", "compile.py")
    return subprocess.check_output(
        [sys.executable, compile_py, mode], cwd=ROOT, text=True, encoding="utf-8"
    )


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

    operations = re.findall(
        r'^\s*\{(PVE_OPERATION_[A-Z0-9_]+),\s*"([^"]+)",\s*"([^"]+)",\s*'
        r'(PVE_MODE_[A-Z]+),\s*([^}]+)\},',
        shared, re.MULTILINE,
    )
    if len(operations) != 9:
        fail(f"expected 9 operations, found {len(operations)}")
    if len({row[0] for row in operations}) != 9 or len({row[1] for row in operations}) != 9:
        fail("operation IDs and display names must be unique")
    shared_header = open(os.path.join(ROOT, "src/game/pve_roguelite.h"), encoding="utf-8").read()
    operation_enum = re.search(
        r"enum EPveOperation\s*\{(.*?)NUM_PVE_OPERATIONS,\s*\};",
        shared_header, re.DOTALL,
    )
    if not operation_enum:
        fail("operation enum or count sentinel missing")
    enum_symbols = re.findall(r"PVE_OPERATION_[A-Z0-9_]+", operation_enum.group(1))
    if enum_symbols != [row[0] for row in operations]:
        fail("operation enum IDs and definition table order differ")
    expected_modes = ["PVE_MODE_INVASION"] * 3 + ["PVE_MODE_HORDE"] * 3 + ["PVE_MODE_EXTRACTION"] * 3
    if [row[3] for row in operations] != expected_modes:
        fail("operations must remain grouped as three routes per PvE mode")
    if any(len([value for value in row[4].split(",") if value.strip()]) != 7 for row in operations):
        fail("every operation must define all seven sidegrade multipliers")

    network = open(os.path.join(ROOT, "datasrc/network.py"), encoding="utf-8").read()
    for message in (
        "Sv_PveProgress", "Sv_PveChoice", "Sv_PvePerk", "Sv_PveContractVote",
        "Sv_PveContractStatus", "Sv_PveResearchReward", "Sv_PveValidation",
        "Sv_PveBuildState", "Cl_PveProgress", "Cl_PveChoice", "Cl_PveContractVote",
        "Cl_PveResearchBuy", "Cl_PveDroneModule", "Sv_PveInvasionRetryVote",
        "Sv_PveInvasionRetryResult", "Cl_PveInvasionRetryVote",
        "Sv_PveOperationVote", "Cl_PveOperationVote", "Sv_PveOperationState",
    ):
        if f'NetMessage("{message}"' not in network:
            fail(f"missing protocol message {message}")
    operation_messages = (
        "Sv_PveOperationVote", "Cl_PveOperationVote", "Sv_PveOperationState",
    )
    message_names = re.findall(r'^\s*NetMessage\("([^"]+)"', network, re.MULTILINE)
    if tuple(message_names[-3:]) != operation_messages:
        fail("operation protocol messages must be appended at the end in v9 order")
    expected_operation_fields = {
        "Sv_PveOperationVote": (
            'NetIntAny("m_Nonce"),', 'NetIntAny("m_EndTick"),',
            'NetIntRange("m_Operation0",0,8),', 'NetIntRange("m_Operation1",0,8),',
            'NetIntRange("m_Votes0",0,\'MAX_CLIENTS\'),',
            'NetIntRange("m_Votes1",0,\'MAX_CLIENTS\'),',
        ),
        "Cl_PveOperationVote": (
            'NetIntAny("m_Nonce"),', 'NetIntRange("m_Choice",0,1),',
        ),
        "Sv_PveOperationState": (
            'NetIntRange("m_Operation",-1,8),', 'NetIntRange("m_State",0,1),',
        ),
    }
    for name, fields in expected_operation_fields.items():
        if message_fields(network, name) != fields:
            fail(f"operation protocol field signature changed: {name}")

    generated_header = generate_protocol("network_header")
    generated_source = generate_protocol("network_source")
    enum_positions = [generated_header.find(f"NETMSGTYPE_{name.upper()},") for name in operation_messages]
    if any(position < 0 for position in enum_positions) or enum_positions != sorted(enum_positions):
        fail("generated operation message IDs do not match datasrc order")
    for name in operation_messages:
        if f"struct CNetMsg_{name}" not in generated_header:
            fail(f"generated protocol header missing {name}")
        if f"case NETMSGTYPE_{name.upper()}:" not in generated_source:
            fail(f"generated protocol source missing {name}")
    if 'if(pMsg->m_Operation < -1 || pMsg->m_Operation > 8)' not in generated_source:
        fail("generated operation state does not preserve the -1 none sentinel")
    for field in ("m_ResearchMask0", "m_ResearchMask1", "m_ResearchMask2", "m_ResearchMask3"):
        if network.count(field) != 2:
            fail(f"128-bit progress field {field} missing from one direction")
    for boundary in (
        'NetIntRange("m_Card0", 0, 102)', 'NetIntRange("m_Contract0", 0, 19)',
        'NetIntRange("m_Card", 7, 99)', 'NetIntRange("m_Module", 1, 3)',
        'NetIntRange("m_Choice", 0, 1)', 'NetIntRange("m_Result", 0, 2)',
    ):
        if boundary not in network:
            fail(f"protocol boundary missing: {boundary}")

    director = open(os.path.join(ROOT, "src/game/server/pve_director.cpp"), encoding="utf-8").read()
    client = open(os.path.join(ROOT, "src/game/client/components/pve_roguelite.cpp"), encoding="utf-8").read()
    invasion = open(os.path.join(ROOT, "src/game/server/gamemodes/invasion.cpp"), encoding="utf-8").read()
    engine_server = open(os.path.join(ROOT, "src/engine/server/server.cpp"), encoding="utf-8").read()
    horde = open(os.path.join(ROOT, "src/game/server/gamemodes/horde.cpp"), encoding="utf-8").read()
    building = open(os.path.join(ROOT, "src/game/server/entities/building.cpp"), encoding="utf-8").read()
    ai = open(os.path.join(ROOT, "src/game/server/ai.cpp"), encoding="utf-8").read()
    client_building = open(os.path.join(ROOT, "src/game/client/components/buildings.cpp"), encoding="utf-8").read()
    radar = open(os.path.join(ROOT, "src/game/client/components/radar.cpp"), encoding="utf-8").read()
    game_client = open(os.path.join(ROOT, "src/game/client/gameclient.cpp"), encoding="utf-8").read()
    controls = open(os.path.join(ROOT, "src/game/client/components/controls.cpp"), encoding="utf-8").read()
    inventory = open(os.path.join(ROOT, "src/game/client/components/inventory.cpp"), encoding="utf-8").read()
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
        "STATE_RETRY_VOTE", "Nonce != m_RetryVoteNonce",
        "m_aRetryVotes[ClientID] != -1", "Retry > Reset",
        "INV_FINAL_ATTEMPT", "INV_FORCE_FLOOR_ONE",
        "PVE_INVASION_RETRY_RESULT_FINAL_FAILURE", "RegenerateMapFromTemplate()",
        "str_copy(g_Config.m_SvMap, g_Config.m_SvInvMap",
        "g_Config.m_SvInvFails == INV_FORCE_FLOOR_ONE && Server()->m_MapGenerated",
    ):
        if token not in invasion:
            fail(f"Invasion retry vote invariant missing: {token}")
    if invasion.count("ClearRun()") != 1:
        fail("Invasion run data must only be cleared when returning to Floor 1")
    if 'str_comp(m_aCurrentMap, "generated") != 0' not in engine_server:
        fail("generated maps can overwrite the original map generation template")
    for token in ("DrawInvasionRetryVote()", "DrawInvasionRetryResult()", "SendInvasionRetryVote"):
        if token not in client:
            fail(f"Invasion retry client overlay missing: {token}")
    for token in (
        "m_Type == BUILDING_SWITCH && !m_PveSwitchActive",
        "m_Collision = Active",
    ):
        if token not in building:
            fail(f"hidden switch authority invariant missing: {token}")
    for token in (
        "SetReactorDefenseActive(true)", "m_pReactor->Activate",
        "m_pReactor->Deactivate()",
    ):
        if token not in invasion:
            fail(f"Invasion reactor objective lifecycle missing: {token}")
    for token in (
        "m_PveReactorObjective && Damage > 0",
        "m_Collision = true", "m_Life = m_MaxLife",
    ):
        if token not in building:
            fail(f"Invasion reactor damage authority missing: {token}")
    for token in (
        "PrioritizeReactorObjective()", "ShootAtClosestBuilding(true)",
        "SeekClosestReactor()",
    ):
        if token not in ai:
            fail(f"Invasion reactor AI priority missing: {token}")
    for token in (
        "const bool Objective = s & (1<<BSTATUS_ON)",
        "Objective ? 480 + ObjectivePulse * 80 : 320",
        "Graphics()->LinesDraw(aLines, Segments + 5)",
    ):
        if token not in client_building:
            fail(f"Invasion reactor objective visualization missing: {token}")
    if 'str_comp(ServerInfo.m_aGameType, "INV") == 0' not in radar:
        fail("Invasion reactor radar emphasis missing")
    for token in (
        "GameplayInputFullyCaptured()", "GameplayInputFullyCaptured() || m_pInventory->IsVisible()",
    ):
        if token not in game_client:
            fail(f"partial inventory input capture missing: {token}")
    for token in (
        "m_pClient->GameplayInputFullyCaptured()", "ReleaseInputCounter(&m_InputData.m_Fire)",
        "ReleaseInputCounter(&m_InputData.m_NextWeapon)", "m_pClient->m_pInventory->IsVisible()",
    ):
        if token not in controls:
            fail(f"inventory combat suppression missing: {token}")
    for token in (
        '"+left", "+right", "+down", "+jump"',
        '"+gamepadleft", "+gamepadright", "+gamepaddown", "+gamepadjump"',
    ):
        if token not in inventory:
            fail(f"inventory movement pass-through missing: {token}")
    if "if(!m_pClient->GameplayInputCaptured())" not in client:
        fail("focused overlays do not suppress contract/build HUD panels")
    for token in (
        "EnsureDefenseArea(pChr->m_Pos)",
        "distance(Pos, m_DefenseAreaCenter) <= PVE_HORDE_DEFENSE_RADIUS",
    ):
        if token not in horde:
            fail(f"Horde defense area invariant missing: {token}")
    for token in (
        "InHordeDefenseArea(ClientID)",
        "InHordeDefenseArea(To)",
    ):
        if token not in director:
            fail(f"Hold the Line area gate missing: {token}")
    for token in (
        'str_comp(ServerInfo.m_aGameType, "HORDE") == 0',
        "PVE_HORDE_DEFENSE_RADIUS",
        "Graphics()->LinesDraw(aLines, NumLines)",
    ):
        if token not in radar:
            fail(f"Horde defense area visualization missing: {token}")

    legacy_messages = (
        "Sv_Broadcast", "Sv_GameVote", "Sv_GameVoteStatus", "Sv_Chat", "Sv_KillMsg",
        "Sv_SoundGlobal", "Sv_TuneParams", "Sv_ExtraProjectile", "Sv_ReadyToEnter",
        "Sv_WeaponPickup", "Sv_Emoticon", "Sv_VoteClearOptions", "Sv_VoteOptionListAdd",
        "Sv_VoteOptionAdd", "Sv_VoteOptionRemove", "Sv_VoteSet", "Sv_VoteStatus",
        "Sv_Inventory", "Cl_Say", "Cl_SetTeam", "Cl_SetSpectatorMode", "Cl_StartInfo",
        "Cl_ChangeInfo", "Cl_Kill", "Cl_Emoticon", "Cl_DropWeapon", "Cl_SelectItem",
        "Cl_UseKit", "Cl_Vote", "Cl_VoteGameMode", "Cl_CallVote", "Cl_InventoryAction",
        "Sv_PveProgress", "Sv_PveChoice", "Sv_PvePerk", "Sv_PveContractVote",
        "Sv_PveContractStatus", "Sv_PveResearchReward", "Sv_PveValidation",
        "Sv_PveBuildState", "Cl_PveProgress", "Cl_PveChoice", "Cl_PveContractVote",
        "Cl_PveResearchBuy", "Cl_PveDroneModule", "Sv_PveInvasionRetryVote",
        "Sv_PveInvasionRetryResult", "Cl_PveInvasionRetryVote",
    )
    if tuple(message_names[:len(legacy_messages)]) != legacy_messages:
        fail("legacy protocol message IDs changed")

    config = open(os.path.join(ROOT, "src/engine/shared/config_variables.h"), encoding="utf-8").read()
    if 'cl_pve_research_mask, 33, "00000000000000000000000000000000"' not in config:
        fail("research mask config is not 128-bit hexadecimal")
    for token in (
        "MACRO_CONFIG_INT(SvPveOperations, sv_pve_operations, 1, 0, 1, CFGFLAG_SERVER",
        "MACRO_CONFIG_INT(SvPveOperationVoteTime, sv_pve_operation_vote_time, 10, 3, 60, CFGFLAG_SERVER",
    ):
        if token not in config:
            fail(f"operation config invariant missing: {token}")

    operation_server_tokens = (
        "BeginOperationVote(ContractVote, PerkChoice)", "FinishOperationVote()",
        "OnOperationVote(int ClientID, int Nonce, int OperationID)",
        "CNetMsg_Sv_PveOperationVote", "CNetMsg_Sv_PveOperationState",
        "PVE_INTERMISSION_OPERATION", "m_LastOperationNonce",
    )
    for token in operation_server_tokens:
        if token not in director and token not in open(os.path.join(ROOT, "src/game/server/pve_director.h"), encoding="utf-8").read():
            fail(f"operation server integration missing: {token}")
    operation_client_tokens = (
        "NETMSGTYPE_SV_PVEOPERATIONVOTE", "NETMSGTYPE_SV_PVEOPERATIONSTATE",
        "CNetMsg_Cl_PveOperationVote", "DrawOperationVote",
    )
    client_header = open(os.path.join(ROOT, "src/game/client/components/pve_roguelite.h"), encoding="utf-8").read()
    for token in operation_client_tokens:
        if token not in client and token not in client_header:
            fail(f"operation client integration missing: {token}")
    version = open(os.path.join(ROOT, "src/game/version.h"), encoding="utf-8").read()
    if '"pve-director-v9"' not in version:
        fail("network protocol version was not advanced to v9")

    print(f"OK: 100 cards (12 new base, 48 new research, 8 legendary), 20 contracts, {new_research_cost} new research points, v9 protocol")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except AssertionError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
