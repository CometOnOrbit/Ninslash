#!/usr/bin/env python3
"""Apply the canonical, review-friendly layout to official weapon Lua."""

import argparse
import pathlib
import re

from weapon_lua_files import official_weapon_paths


LEVEL_CURVE = re.compile(r'^(\s*)([a-z0-9_]+) = weapon\.curve\.levels \{([^}]*)\},$')
BOOLEAN_FIELDS = {
	"full_auto", "uses_ammo", "valid_for_turret", "explosive_projectile",
	"laser_weapon", "aimline", "auto_pick",
}


def format_text(text: str) -> str:
	result = []
	for line in text.splitlines():
		curve = LEVEL_CURVE.match(line)
		if curve:
			indent, field, raw_values = curve.groups()
			values = [value.strip() for value in raw_values.split(",")]
			if len(values) > 4:
				result.append(f"{indent}{field} = weapon.curve.levels {{")
				for offset in range(0, len(values), 4):
					result.append(f"{indent}  " + ", ".join(values[offset:offset + 4]) + ",")
				result.append(f"{indent}}},")
				continue

		field = re.match(r'^(\s*)([a-z0-9_]+) = ([01]),$', line)
		if field and field.group(2) in BOOLEAN_FIELDS:
			indent, name, value = field.groups()
			line = f"{indent}{name} = {'true' if value == '1' else 'false'},"
		result.append(line)
	return "\n".join(result) + "\n"


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("--write", action="store_true")
	args = parser.parse_args()
	root = pathlib.Path(__file__).resolve().parents[1]
	official_paths = official_weapon_paths(root)
	paths = [root / "data" / "weapons" / "weapon_dsl.lua", *official_paths]
	dirty = []
	invalid = []
	for path in paths:
		text = path.read_text(encoding="utf-8")
		if path in official_paths and "weapon.curve.levels" in text:
			invalid.append(f"{path.relative_to(root)}: explicit 16-value curves are not allowed in official content")
		for line_number, line in enumerate(text.splitlines(), 1):
			if "\t" in line or len(line) > 120:
				invalid.append(f"{path.relative_to(root)}:{line_number}: line must use spaces and stay within 120 columns")
		formatted = format_text(text)
		if formatted != text:
			dirty.append(path.relative_to(root))
			if args.write:
				path.write_text(formatted, encoding="utf-8")
	if invalid:
		for message in invalid:
			print(message)
		return 1
	if dirty and not args.write:
		for path in dirty:
			print(path)
		return 1
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
