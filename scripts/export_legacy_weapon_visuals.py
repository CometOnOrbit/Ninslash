#!/usr/bin/env python3
"""Export the final pre-Lua player weapon visuals as compact Lua overrides."""

import argparse
import math
import pathlib
import re
import struct
import subprocess
import sys


FLOAT_FIELDS = {
	"render_offset_x", "render_offset_y", "muzzle_offset_x", "muzzle_offset_y",
	"projectile_offset_x", "projectile_offset_y", "hand_offset_x", "hand_offset_y",
	"color_swap_x", "color_swap_y", "render_recoil", "projectile_size",
	"projectile_sprite", "trace_threshold", "screenshake_amount",
}
MELEE_STATIC = {"TOOL", "CHAINSAW", "CLAW"}
EXPLOSIVE_STATIC = {"GRENADE1", "GRENADE2", "GRENADE3", "BAZOOKA", "BOUNCER", "CLUSTER", "BOMB"}
PAIR_FIELDS = (
	("visual_size", "visual_size_x", "visual_size_y"),
	("visual_size2", "visual_size2_x", "visual_size2_y"),
	("render_offset", "render_offset_x", "render_offset_y"),
	("muzzle_offset", "muzzle_offset_x", "muzzle_offset_y"),
	("projectile_offset", "projectile_offset_x", "projectile_offset_y"),
	("hand_offset", "hand_offset_x", "hand_offset_y"),
	("color_swap", "color_swap_x", "color_swap_y"),
)
VISUAL_FIELDS = (
	"render_type", "visual_size_x", "visual_size_y", "visual_size2_x", "visual_size2_y",
	"render_offset_x", "render_offset_y", "muzzle_offset_x", "muzzle_offset_y",
	"projectile_offset_x", "projectile_offset_y", "hand_offset_x", "hand_offset_y",
	"color_swap_x", "color_swap_y", "render_recoil", "projectile_size", "projectile_sprite",
	"projectile_trace_type", "trace_threshold", "explosion_sprite", "explosion_sound",
	"fire_sound", "fire_sound2", "muzzle_type", "muzzle_amount", "screenshake_amount",
)
MECHANIC_FIELDS = (
	"firing_type", "full_auto", "uses_ammo", "shot_spread", "projectile_spread",
	"projectile_curvature", "burst_count", "burst_reload", "valid_for_turret",
	"electro_amount", "explosive_projectile", "laser_weapon", "aimline",
	"projectile_pos_type", "laser_range", "laser_charge", "projectile_bounces",
)
STATIC_IDS = (
	"tool", "gun1", "gun2", "grenade1", "grenade2", "grenade3", "bazooka", "bouncer",
	"chainsaw", "flamer", "upgrade", "shield", "respawner", "mask1", "mask2", "mask3",
	"mask4", "mask5", "invis", "electrowall", "areashield", "syringe", "cluster",
	"shuriken", "claw", "bomb", "ball",
)
MODULAR_IDS = tuple(
	f"{part1}-{part2}"
	for part1, part2s in (
		*( (f"base{index}", ("barrel1", "barrel2", "barrel3", "barrel4", "charge", "capacitor", "rail")) for index in range(1, 7) ),
		("melee", ("melee1", "melee2", "melee3", "melee4", "melee5", "melee6")),
		("spin", ("melee1", "melee2", "melee3", "melee4", "melee5", "melee6")),
	)
	for part2 in part2s
)


def f32(value):
	return struct.unpack("f", struct.pack("f", float(value)))[0]


def close(left, right):
	return abs(float(left) - float(right)) <= 1e-6


def number(value):
	if isinstance(value, bool):
		return "true" if value else "false"
	if isinstance(value, int) or float(value).is_integer():
		return str(int(value))
	return format(float(value), ".7g")


def load_legacy(root):
	source = subprocess.check_output(
		["git", "show", "HEAD:datasrc/weapon_profiles.py"], cwd=root, text=True)
	sys.path.insert(0, str(root / "datasrc"))
	namespace = {"__name__": "legacy_weapon_profiles"}
	exec(compile(source, "legacy_weapon_profiles.py", "exec"), namespace)
	return namespace


def final_value(api, profile, key, field, level):
	value = api["resolve"](profile.values.get(field, 0), level)
	if field != "render_recoil" or not value:
		return value
	if key[0] == "modular" or key[1] in MELEE_STATIC:
		melee = key[0] == "modular" and key[1] in api["MELEE_PART1_NAMES"] or key[1] in MELEE_STATIC
	else:
		melee = False
	if melee:
		factor = 1.10
	elif api["resolve"](profile.values.get("explosive_projectile", 0), level) or (key[0] == "static" and key[1] in EXPLOSIVE_STATIC):
		factor = 1.15
	elif api["resolve"](profile.values.get("full_auto", 0), level):
		factor = 1.05
	else:
		factor = 1.15
	return min(20.0, f32(float(value) * factor))


def curve(values, max_level):
	if all(close(value, values[0]) for value in values):
		return number(values[0])
	if max_level:
		base, amount = values[0], f32(float(values[max_level]) - float(values[0]))
		predicted = [f32(f32(base) + f32(f32(level / max_level) * f32(amount))) for level in range(16)]
		if all(close(actual, expected) for actual, expected in zip(values, predicted)):
			return f"weapon.curve.linear({number(base)}, {number(amount)}, {max_level})"
	step = f32(float(values[1]) - float(values[0]))
	predicted = [f32(f32(values[0]) + f32(f32(level) * step)) for level in range(16)]
	if all(close(actual, expected) for actual, expected in zip(values, predicted)):
		return f"weapon.curve.step({number(values[0])}, {number(step)})"
	for level in range(1, 16):
		if all(close(values[index], values[0] if index < level else values[level]) for index in range(16)):
			return f"weapon.curve.switch({level}, {number(values[0])}, {number(values[level])})"
	for level in range(16):
		others = [values[index] for index in range(16) if index != level]
		if all(close(value, others[0]) for value in others):
			return f"weapon.curve.at({level}, {number(others[0])}, {number(values[level])})"
	raise ValueError("visual curve cannot be represented by the official formula helpers")


def stable_id(key):
	if key[0] == "static":
		return f"official:static:{key[1].lower()}"
	return f"official:modular:{key[1].lower()}-{key[2].lower()}"


def baseline_player_profiles(root):
	text = (root / "tests" / "weapon_visual_baseline.inc").read_text(encoding="utf-8")
	section = text.split("static const float gs_aLegacyPlayerVisuals", 1)[1].split("};", 1)[0]
	rows = []
	for match in re.finditer(r"\{([^{}]+)\},", section):
		values = [float(value.strip()) for value in match.group(1).split(",")]
		if len(values) == len(VISUAL_FIELDS):
			rows.append(values)
	if len(rows) != 81 * 16:
		raise ValueError("invalid checked-in player visual baseline")
	ids = [*(f"official:static:{name}" for name in STATIC_IDS), *(f"official:modular:{name}" for name in MODULAR_IDS)]
	return [
		(stable_id, rows[index * 16:(index + 1) * 16], 0 if index < len(STATIC_IDS) else 4)
		for index, stable_id in enumerate(ids)
	]


def generate(root):
	lines = [
		"-- Final player-weapon visuals from the pre-Lua profile generator.",
		"-- Kept separate so combat formulas cannot alter established presentation.",
		"",
	]
	for stable_id_value, rows, max_level in baseline_player_profiles(root):
		values = {field: [row[index] for row in rows] for index, field in enumerate(VISUAL_FIELDS)}
		lines.append(f'weapon.override_visuals("{stable_id_value}", {{')
		consumed = set()
		for name, x_field, y_field in PAIR_FIELDS:
			x, y = curve(values[x_field], max_level), curve(values[y_field], max_level)
			lines.append(f"  {name} = {{{x}, {y}}},")
			consumed.update((x_field, y_field))
		for field in VISUAL_FIELDS:
			if field in consumed:
				continue
			lines.append(f"  {field} = {curve(values[field], max_level)},")
		lines.extend(("})", ""))
	return "\n".join(lines)


def generate_header(root):
	api = load_legacy(root)
	fields = api["VISUAL_FIELDS"]
	rows = []
	for key, profile in api["ordered_player_profiles"]():
		for level in range(16):
			values = [final_value(api, profile, key, field, level) for field in fields]
			rows.append("\t{" + ", ".join(number(value) for value in values) + "},")
	lines = [
		"// Generated by scripts/export_legacy_weapon_visuals.py. Do not edit.",
		"static const float gs_aLegacyPlayerVisuals[WEAPON_PROFILE_COUNT][27] = {",
		*rows,
		"};",
		"",
	]
	for symbol, names, profiles in (
		("Droid", api["DROID_NAMES"], api["DROID_PROFILES"]),
		("DroidDeath", api["DROID_NAMES"], api["DROID_DEATH_PROFILES"]),
		("Building", api["BUILDING_NAMES"], api["BUILDING_PROFILES"]),
	):
		lines.append(f"static const float gs_aLegacy{symbol}Visuals[{len(names)}][27] = {{")
		for name in names:
			profile = profiles[name]
			values = [api["resolve"](profile.values.get(field, 0), 0) for field in api["VISUAL_FIELDS"]]
			lines.append("\t{" + ", ".join(number(value) for value in values) + "},")
		lines.extend(("};", ""))
	lines.append(f"static const float gs_aLegacyRangedMechanics[{42 * 16}][{len(MECHANIC_FIELDS)}] = {{")
	for part1 in (f"BASE{index}" for index in range(1, 7)):
		for part2 in ("BARREL1", "BARREL2", "BARREL3", "BARREL4", "CHARGE", "CAPACITOR", "RAIL"):
			profile = api["PLAYER_PROFILES"][("modular", part1, part2)]
			for level in range(16):
				values = [api["resolve"](profile.values.get(field, 0), level) for field in MECHANIC_FIELDS]
				lines.append("\t{" + ", ".join(number(value) for value in values) + "},")
	lines.extend(("};", ""))
	return "\n".join(lines)


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("--write", action="store_true")
	parser.add_argument("--import-legacy", action="store_true")
	args = parser.parse_args()
	root = pathlib.Path(__file__).resolve().parents[1]
	path = root / "data" / "weapons" / "official" / "visuals.lua"
	header_path = root / "tests" / "weapon_visual_baseline.inc"
	if args.import_legacy:
		header_path.write_text(generate_header(root), encoding="utf-8")
	generated = generate(root)
	if args.write:
		path.write_text(generated, encoding="utf-8")
		return 0
	dirty = []
	if not path.is_file() or path.read_text(encoding="utf-8") != generated:
		dirty.append(path.relative_to(root))
	if dirty:
		for dirty_path in dirty:
			print(dirty_path)
		return 1
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
