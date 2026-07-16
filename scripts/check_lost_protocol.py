#!/usr/bin/env python3
"""Behavioral integration gates for the Lost Protocol PvE expansion."""

from pathlib import Path
import json
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
    require('NetIntRange("m_PveCargo", 0, 3)' in network, "dedicated PvE cargo character field missing")
    for field in ("m_Step", "m_Progress", "m_Target", "m_TargetType", "m_TargetX", "m_TargetY", "m_CargoCarrier"):
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
        "droid_siege_engine.cpp": ("OnHealthThreshold", "CBulwark"),
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
    require("MovementTick" in specialist_base and "MoveBox" in specialist_base and "m_PlacementResolved" in specialist_base,
            "specialist collision/embedded-spawn resolution missing")
    for token in ("AcquireTarget(m_IsBoss ? 1400.0f : 1100.0f, false)", "FloorAhead", "TargetAbove", "LongPursuit", "Stalled", "m_NextHopTick", "DeathAge", "Settled"):
        require(token in specialist_base, f"crawler-style pursuit/death behavior missing: {token}")
    strength_tokens = {
        "droid_bulwark.cpp": ("FireProjectile(28", "TickSpeed() * 3 / 10"),
        "droid_assembler.cpp": ("FireProjectile(22", "TickSpeed() / 2"),
        "droid_saboteur.cpp": ("FireProjectile(24", "TickSpeed() * 2 / 5", "TickSpeed() * 3 / 4"),
        "droid_railgunner.cpp": ("56", "TickSpeed() / 4", "TickSpeed() / 2"),
    }
    for filename, tokens in strength_tokens.items():
        source = read(f"src/game/server/entities/{filename}")
        for token in tokens:
            require(token in source, f"{filename} combat pressure missing: {token}")
    overseer = read("src/game/server/entities/droid_overseer_core.cpp")
    for token in ("m_OrbitAngle", "Desired", "MaxSpeed", "MoveBox", "BeforeHit", "Side * Sign", "for(int i = 0; i < 8; i++)", "BUILDING_PVE_SHIELD_NODE", "Protects(GameWorld(), this)"):
        require(token in overseer, f"Overseer flight/attack behavior missing: {token}")
    require("m_Vel.y +=" not in overseer, "Overseer flight still applies ground gravity")
    require("CollisionSize(), 0, false, true" in overseer, "Overseer ignores one-way platform collision")
    railgunner = read("src/game/server/entities/droid_railgunner.cpp")
    require("FindEntities(From, 0" not in railgunner, "Railgunner still uses an empty zero-radius damage query")
    require("GetPlayerChar(ClientID)" in railgunner and "closest_point_on_line" in railgunner,
            "Railgunner penetration does not enumerate player line targets")
    require("FindFirst(CGameWorld::ENTTYPE_DROID)" not in railgunner, "Railgunner still damages allied mechanical units")
    # Warning and recoil are separate: the telegraph must not play a fire animation.
    require("m_AttackTick = m_ChargeStart" not in railgunner and "FireRail(); m_AttackTick = Server()->Tick()" in railgunner,
            "Railgunner telegraph still triggers the firing animation")
    siege = read("src/game/server/entities/droid_siege_engine.cpp")
    for token in ("CSiegeStrike", "Server()->TickSpeed()", "m_ChargeEndTick", "CSiegeMine", "OnSpecialistDeath", "m_apGuards"):
        require(token in siege, f"Siege Engine staged skill/cleanup missing: {token}")

    operation = read("src/game/server/pve_operation_director.cpp")
    for token in (
        "PVE_OPERATION_FOUNDRY_SHUTDOWN && m_Stage == 2",
        "EVENT_BOSS", "mode-objective-flow",
        "OnOperationChainFinished", "FindEscape(&ExitPos)",
        "OnOperationChainFailed",
    ):
        require(token in operation, f"operation lifecycle gate missing: {token}")
    require("BeginRisingAcid" not in operation, "an operation still starts native rising acid")
    require("acid escape deadline" not in operation, "Foundry operation still owns an acid deadline")
    operation_header = read("src/game/server/pve_operation_director.h")
    require("OverridesModeFlow" not in operation_header,
            "operation can still replace the native mode flow")
    invasion = read("src/game/server/gamemodes/invasion.cpp")
    for token in ("RouteQuestActive", "TryStartRouteQuest", "FinishOperationFloor", "SyncRouteQuestFromOperation"):
        require(token in invasion, f"Invasion route quest lifecycle missing: {token}")
    require("OnOperationChainAbandoned" not in invasion.split("void CGameControllerInvasion::NextLevel", 1)[1].split("void CGameControllerInvasion::ChangeQuest", 1)[0],
            "entering the door still abandons an active route quest")
    require("OperationOverrides" not in invasion,
            "Invasion primary quest flow can still be disabled by an operation")
    queue_next_start = invasion.index("void CGameControllerInvasion::QueueNextObjectiveQuest()")
    queue_next_end = invasion.index("void CGameControllerInvasion::OnSwitchTriggered()", queue_next_start)
    require("PVE_OPERATION_" not in invasion[queue_next_start:queue_next_end],
            "Operation still changes Invasion's native quest sequence")
    require("const int Remaining = max(0, m_EnemiesLeft) + CountBotsAlive()" in invasion and
            "if(Remaining == 0)" in invasion,
            "Purge HUD and completion still use different enemy counts")
    purge_block = invasion.index("if (m_Quest == QUEST_KILLREMAININGENEMIES)")
    survive_block = invasion.rfind("if (m_Quest == QUEST_SURVIVEWAVE || m_Quest == QUEST_SURVIVEWAVETIME)", 0, purge_block)
    require(invasion[survive_block:purge_block].rstrip().endswith("}"),
            "Purge completion is incorrectly nested inside the wave-survival branch")
    for controller_name in ("invasion", "horde", "extract"):
        controller_source = read(f"src/game/server/gamemodes/{controller_name}.cpp")
        require("OperationIntermission" in controller_source and "if(!OperationIntermission)" in controller_source,
                f"{controller_name} starts or advances operations while the intermission is paused")
    require("StartIntermission(g_Config.m_SvMapGenLevel % 3 == 0, true, OperationVote)" in invasion,
            "Invasion operation vote is not gated behind an explicit OperationVote flag")
    require("INVASION_THEME_ACID_ESCAPE" in invasion and
            "InvasionThemeFromLevel(g_Config.m_SvMapGenLevel) != INVASION_THEME_ACID_ESCAPE" in invasion,
            "acid-escape floors can still enter an operation vote")
    pve_director = read("src/game/server/pve_director.cpp")
    pve_shared = read("src/game/pve_roguelite.h")
    require('str_find_nocase(g_Config.m_SvInvMap, "foundry")' not in pve_director,
            "operations are still gated behind Foundry map names")
    require("QUEST_ROUTE" in read("src/game/questinfo.h"),
            "route quest type missing")
    require("RouteQuestActive" in invasion and "TryStartRouteQuest" in invasion and
            "TriggerEscape()" in invasion.split("FinishOperationFloor", 1)[1],
            "invasion route quest flow does not open the door after completion")
    require("PVE_OPERATION_STATE_SELECTED" in pve_shared, "operation selection is not distinct from an active mission")
    require("m_OperationState = PVE_OPERATION_STATE_SELECTED" in pve_director and
            "if(m_OperationState == PVE_OPERATION_STATE_SELECTED)" in pve_director,
            "operation becomes active before the intermission queue closes")
    controller = read("src/game/server/gamecontroller.cpp")
    require("max(1, m_RisingAcidDuration)" in controller, "rising acid ignores its requested duration")
    cargo_target = read("src/game/server/entities/pve_operation_target.cpp")
    require("GivePveCargo" in cargo_target and "HasPveCargo" in cargo_target, "operation cargo lifecycle is not independent")
    require("GiveBomb" not in cargo_target and "IsBombCarrier" not in cargo_target, "operation cargo still reuses the CS bomb")
    for token in ("BuildingTypeForTarget", "IsDestructible", "DeactivateRadar", "if(!IsDestructible() && Damage > 0)"):
        require(token in cargo_target, f"operation target semantics/cleanup missing: {token}")
    require("DefendTerminal" in operation and "m_NextReinforcementTick" in operation,
            "timed defense stages do not replenish attackers")
    require("m_EndTick = m_pGameServer->Server()->Tick() + max(0, m_Required - m_Progress)" in operation,
            "timed defense HUD deadline is detached from occupied progress")
    require("SpawnOperationOrdinaryEnemies" not in operation and "ClearOperationOrdinaryEnemies" not in operation,
            "operation still changes the native Invasion enemy lifecycle")
    horde = read("src/game/server/gamemodes/horde.cpp")
    require("if(m_Wave > 0 && m_Wave % 4 == 0)" in horde,
            "Operation can still suppress Horde's native fourth-wave Boss")
    require((ROOT / "data/pve_cargo.png").is_file(), "PvE cargo atlas missing")
    require((ROOT / "data/generator_shield.png").is_file(), "generator shield bitmap missing")
    require((ROOT / "data/pve_shield_relay.png").is_file(), "shield relay bitmap missing")
    require((ROOT / "data/pve_objectives.png").is_file(), "operation objective bitmap atlas missing")
    require((ROOT / "data/generator_shield.png").read_bytes() != (ROOT / "data/pve_shield_relay.png").read_bytes(),
            "shield relay overwrote the legacy generator shield")
    content = read("datasrc/content.py")
    require('Image("pve_cargo", "pve_cargo.png")' in content and 'Sprite("pve_cargo_coolant"' in content, "PvE cargo atlas is not registered")
    require('Image("pve_objectives", "pve_objectives.png")' in content and 'Sprite("pve_overload_terminal"' in content,
            "operation objective bitmap atlas is not registered")
    require('Image("pve_shield_relay", "pve_shield_relay.png")' in content and 'Sprite("pve_shield_relay"' in content,
            "shield relay bitmap is not registered separately")
    cargo_client = read("src/game/client/render.cpp") + read("src/game/client/components/pve_roguelite.cpp")
    require("IMAGE_PVE_CARGO" in cargo_client and "SPRITE_PVE_CARGO_COOLANT" in cargo_client, "PvE cargo world/carrier rendering missing")
    require("MapscreenToWorld" in cargo_client, "PvE cargo/drone rendering does not restore world coordinates")
    objective_client = read("src/game/client/components/buildings.cpp")
    for token in ("IMAGE_PVE_SHIELD_RELAY", "SPRITE_PVE_SHIELD_RELAY", "IMAGE_PVE_OBJECTIVES", "SPRITE_PVE_OVERLOAD_TERMINAL",
                  "SPRITE_PVE_ASSEMBLY_NODE", "SPRITE_PVE_TARGETING_BEACON", "SPRITE_PVE_UPLOAD_POINT",
                  "IMAGE_PVE_CARGO", "SPRITE_PVE_CARGO_DATA", "SPRITE_PVE_CARGO_ENERGY"):
        require(token in objective_client, f"formal operation objective bitmap missing: {token}")
    require("auto Quad =" not in objective_client, "operation objective bodies still use procedural Quad drawing")
    target_server = read("src/game/server/entities/pve_operation_target.cpp")
    for token in ("BUILDING_PVE_SHIELD_RELAY", "BUILDING_PVE_OVERLOAD_TERMINAL", "BUILDING_PVE_ASSEMBLY_NODE",
                  "BUILDING_PVE_TARGETING_BEACON", "BUILDING_PVE_DATA_CORE", "BUILDING_PVE_UPLOAD_POINT",
                  "BUILDING_PVE_SHIELD_NODE", "BUILDING_PVE_ENERGY_CORE"):
        require(token in target_server and token in objective_client, f"PvE building lacks formal snapshot mapping: {token}")
    offscreen = read("src/engine/client/backend_sdl.cpp")
    require("NINSLASH_OFFSCREEN" in offscreen and "SDL_WINDOW_HIDDEN" in offscreen, "SDL OpenGL offscreen capture path missing")

    drone = read("src/game/server/entities/pve_drone.cpp")
    require("CNetObj_PveDrone" in drone and "CNetObj_Laser" not in drone, "drone still uses laser snapshot")
    require("TickSpeed() * 12" in drone and "m_Health = 40" in drone, "drone rebuild/health contract missing")

    client = read("src/game/client/components/pve_roguelite.cpp")
    require("pDef->m_apSteps[Step]" in client, "operation vote lacks three-step preview")
    require("m_OperationTargetPos - LocalPos" in client, "operation direction HUD missing")
    droid_client = read("src/game/client/components/droids.cpp")
    require('pCurrent->m_Anim == 1 ? "move"' in droid_client and 'pCurrent->m_Anim == 2 ? "fly"' in droid_client,
            "Lost Protocol locomotion animations are not selected")
    require("pCurrent->m_Angle" in droid_client, "Lost Protocol weapon bones ignore continuous aim angle")
    render = read("src/game/client/render.cpp") + read("src/game/client/render.h")
    for token in ("pBaseAnimation", "ApplyAnimation(pBaseAnimation, BaseTime)", "pBaseAnim", "m_LocomotionTime", "m_SmoothedAimAngle"):
        require(token in render + droid_client, f"Lost Protocol locomotion/action blending missing: {token}")

    text = read("src/engine/client/text.cpp")
    for token in ("IsCjk", "IsOpeningPunctuation", "IsClosingPunctuation"):
        require(token in text, f"UTF-8 layout helper missing: {token}")

    for mode in ("invasion", "horde", "extract"):
        cfg = read(f"cfg/{mode}_foundry.cfg")
        require("sv_map generate_foundry1" in cfg, f"{mode} foundry config missing")
        require("sv_mapgen_random_seed 0" in cfg and "sv_mapgen_seed 1337" in cfg, f"{mode} fixed seed missing")
    require((ROOT / "data/maps/generate_foundry1.map").is_file(), "foundry map missing")

    spine_assets = {
        "bulwark": 10,
        "assembler": 10,
        "saboteur": 9,
        "railgunner": 8,
        "siege_engine": 9,
        "overseer_core": 5,
        "pve_drone_assault": 4,
        "pve_drone_guardian": 4,
        "pve_drone_repair": 5,
    }
    for name, minimum_bones in spine_assets.items():
        document = json.loads(read(f"data/anim/lost_protocol/{name}.json"))
        bones = document.get("bones", [])
        slots = document.get("slots", [])
        animations = document.get("animations", {})
        bone_names = {bone.get("name") for bone in bones}
        require(len(bones) >= minimum_bones, f"{name} is not a component rig")
        require(len(slots) >= 2, f"{name} still uses one full-image attachment")
        require(all(slot.get("bone") != "root" for slot in slots), f"{name} contains an unbound root illustration")
        for animation_name, animation in animations.items():
            unknown = set(animation.get("bones", {})) - bone_names
            require(not unknown, f"{name}/{animation_name} refers to missing bones: {sorted(unknown)}")
        if name in ("bulwark", "assembler", "saboteur", "railgunner", "siege_engine"):
            require("move" in animations, f"{name} has no locomotion animation")
            move_bones = animations["move"].get("bones", {})
            leg_timelines = {bone: timeline for bone, timeline in move_bones.items() if bone.endswith(("_upper", "_lower"))}
            minimum_leg_timelines = 10 if name == "siege_engine" else 8
            require(len(leg_timelines) >= minimum_leg_timelines, f"{name} does not animate every crawler leg")
            for bone, timeline in leg_timelines.items():
                rotations = timeline.get("rotate", [])
                require(len(rotations) >= 7, f"{name}/{bone} locomotion is undersampled")
                first = {key: value for key, value in rotations[0].items() if key != "time"}
                last = {key: value for key, value in rotations[-1].items() if key != "time"}
                require(first == last, f"{name}/{bone} locomotion loop has a visible seam")
            for action in ("attack", "hit", "emp"):
                action_bones = animations.get(action, {}).get("bones", {})
                require(not any(bone.endswith(("_upper", "_lower", "_ankle")) for bone in action_bones),
                        f"{name}/{action} resets crawler legs instead of overlaying locomotion")
            destroyed = animations.get("destroyed", {}).get("bones", {})
            require(destroyed, f"{name} has no destroyed animation")
            require(not any("scale" in timeline for timeline in destroyed.values()), f"{name} death still rubber-scales the body")
            rotations = [frame.get("angle", 0) for timeline in destroyed.values() for frame in timeline.get("rotate", [])]
            require(rotations and max(abs(angle) for angle in rotations) <= 45,
                    f"{name} death uses absolute/setup rotations instead of a modest relative collapse")
        if name == "overseer_core":
            require("fly" in animations, "Overseer Core has no rotating flight animation")
        require((ROOT / f"data/anim/lost_protocol/{name}.atlas").is_file(), f"{name} atlas missing")
        require((ROOT / f"data/anim/lost_protocol/{name}.png").is_file(), f"{name} PNG missing")

    client_assets = read("src/game/client/skelebank.cpp")
    for module in ("assault", "guardian", "repair"):
        require(f"pve_drone_{module}.json" in client_assets, f"{module} drone rig is not loaded")

    print("OK: Lost Protocol v10 behavior, component Spine rigs, operations, UTF-8 layout and foundry integration")


if __name__ == "__main__":
    try:
        main()
    except AssertionError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
