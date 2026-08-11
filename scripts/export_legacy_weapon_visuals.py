#!/usr/bin/env python3
"""Export the final pre-Lua player weapon visuals as compact Lua overrides."""

import argparse
import pathlib
import re
import struct


FLOAT_FIELDS = {
	"render_offset_x", "render_offset_y", "muzzle_offset_x", "muzzle_offset_y",
	"projectile_offset_x", "projectile_offset_y", "hand_offset_x", "hand_offset_y",
	"color_swap_x", "color_swap_y", "render_recoil", "projectile_size",
	"projectile_sprite", "trace_threshold", "screenshake_amount",
}
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
STATIC_IDS = (
	"tool", "gun1", "gun2", "grenade1", "grenade2", "grenade3", "flash_grenade", "blind_grenade",
	"bazooka", "bouncer",
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
	if len(rows) != len(STATIC_IDS + MODULAR_IDS) * 16:
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


def lua_block(text, opening):
	depth = 0
	quote = ""
	for index in range(opening, len(text)):
		character = text[index]
		if quote:
			if character == quote and text[index - 1] != "\\":
				quote = ""
			continue
		if character in "'\"":
			quote = character
		elif character == "{":
			depth += 1
		elif character == "}":
			depth -= 1
			if depth == 0:
				return text[opening + 1:index]
	raise ValueError("unclosed Lua table")


def lua_number(text, field):
	match = re.search(rf"\b{field}\s*=\s*(-?(?:\d+(?:\.\d*)?|\.\d+))\b", text)
	if not match:
		raise ValueError(f"missing Lua visual field: {field}")
	return float(match.group(1))


def droid_visual_rows(root, filename):
	text = (root / "data" / "weapons" / "official" / "attacks" / filename).read_text(encoding="utf-8")
	defaults_text = (root / "data" / "weapons" / "weapon_dsl.lua").read_text(encoding="utf-8")
	defaults = {field: lua_number(defaults_text.split("local visual_defaults = {", 1)[1], field) for field in VISUAL_FIELDS}
	templates = defaults_text.split("weapon.visual = {", 1)[1].split("}", 1)[0]
	template_values = {name: int(value) for name, value in re.findall(r"\b(\w+)\s*=\s*(\d+)", templates)}
	rows = {}
	for match in re.finditer(r"(?:attack_profile\.)?define\s*\{", text):
		body = lua_block(text, match.end() - 1)
		type_match = re.search(r"\btype\s*=\s*(\d+)\b", body)
		name_match = re.search(r"\bname\s*=\s*[\"']([a-z0-9_]+)[\"']", body)
		template_match = re.search(r"\bvisual_template\s*=\s*weapon\.visual\.(\w+)", body)
		if not type_match or not name_match or not template_match:
			raise ValueError(f"incomplete droid visual profile in {filename}")
		visuals_start = body.index("visuals")
		visuals_opening = body.index("{", visuals_start)
		visuals = lua_block(body, visuals_opening)
		values = defaults.copy()
		values["render_type"] = float(template_values[template_match.group(1)])
		for name, x_field, y_field in PAIR_FIELDS:
			pair = re.search(rf"\b{name}\s*=\s*\{{\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*\}}", visuals)
			if pair:
				values[x_field], values[y_field] = float(pair.group(1)), float(pair.group(2))
		for field in VISUAL_FIELDS:
			if field in {x for _, x, _ in PAIR_FIELDS} or field in {y for _, _, y in PAIR_FIELDS}:
				continue
			match = re.search(rf"\b{field}\s*=\s*(-?(?:\d+(?:\.\d*)?|\.\d+))\b", visuals)
			if match:
				values[field] = float(match.group(1))
		type_id = int(type_match.group(1))
		if type_id in rows:
			raise ValueError(f"duplicate droid visual type: {type_id}")
		rows[type_id] = [values[field] for field in VISUAL_FIELDS]
	if sorted(rows) != list(range(17)):
		raise ValueError(f"expected droid visual types 0-16 in {filename}")
	return [rows[type_id] for type_id in range(17)]


def replace_droid_visual_array(text, symbol, rows):
	marker = f"static const float gs_aLegacy{symbol}Visuals[17][27] = {{"
	start = text.index(marker)
	end = text.index("\n};", start) + len("\n};")
	array = marker + "\n" + "\n".join(
		"\t{" + ", ".join(number(value) for value in row) + "}," for row in rows
	) + "\n};"
	return text[:start] + array + text[end:]


def generate_header(root):
	header_path = root / "tests" / "weapon_visual_baseline.inc"
	generated = header_path.read_text(encoding="utf-8")
	for symbol, filename in (("Droid", "droid.lua"), ("DroidDeath", "droid_death.lua")):
		generated = replace_droid_visual_array(generated, symbol, droid_visual_rows(root, filename))
	return generated


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
