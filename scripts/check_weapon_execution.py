#!/usr/bin/env python3
"""Reject content-identity branches from the generic weapon execution path."""

import pathlib
import re
import sys
import ast


FILES = (
	"src/game/server/entities/weapon_behavior.cpp",
	"src/game/server/entities/weapon.cpp",
	"src/game/server/gamecontext.cpp",
	"src/game/server/entities/projectile.cpp",
	"src/game/server/entities/laser.cpp",
	"src/game/client/components/weapons.cpp",
	"src/game/client/components/items.cpp",
	"src/game/client/components/players.cpp",
	"src/game/client/components/effects.cpp",
	"src/game/client/render.cpp",
)
FORBIDDEN = re.compile(r"\bSW_[A-Z0-9_]+\b|m_StaticType\s*==|m_Part[12]\s*==")
DEAD_CHARGE_TAG = re.compile(r"WEAPON_BEHAVIOR_(?:CHARGED_BLADE|CAPACITOR|RAIL)")
COMPONENT_NAME = re.compile(r'\bname\s*=\s*("(?:[^"\\]|\\.)*")')


def main() -> int:
	root = pathlib.Path(__file__).resolve().parents[1]
	failures = []
	component_source = (root / "data/weapons/official/components.lua").read_text(encoding="utf-8")
	component_names = [ast.literal_eval(match.group(1)) for match in COMPONENT_NAME.finditer(component_source)]
	if not component_names:
		failures.append("data/weapons/official/components.lua: no component names found")
	for relative in FILES:
		for line_number, line in enumerate((root / relative).read_text(encoding="utf-8").splitlines(), 1):
			if FORBIDDEN.search(line):
				failures.append(f"{relative}:{line_number}: {line.strip()}")
			if DEAD_CHARGE_TAG.search(line):
				failures.append(f"{relative}:{line_number}: charge mechanics must use Lua combat fields: {line.strip()}")
	for line_number, line in enumerate((root / "src/game/client/components/effects.cpp").read_text(encoding="utf-8").splitlines(), 1):
		if re.search(r"\bPART[12]_[A-Z0-9_]+\b|\.m_Part[12]\b|Source\.m_Type\b|\b(?:DROIDTYPE|BUILDING)_[A-Z0-9_]+\b", line):
			failures.append(f"src/game/client/components/effects.cpp:{line_number}: impact presentation must use Lua visual fields: {line.strip()}")
	for line_number, line in enumerate((root / "src/game/server/gamecontext.cpp").read_text(encoding="utf-8").splitlines(), 1):
		if re.search(r"\bDROIDTYPE_[A-Z0-9_]+\b|Source\.m_Type\s*==", line):
			failures.append(f"src/game/server/gamecontext.cpp:{line_number}: attack execution must use Lua combat fields: {line.strip()}")
	for path in (root / "src").rglob("*"):
		if path.suffix not in (".cpp", ".h"):
			continue
		relative = path.relative_to(root)
		for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
			for component_name in component_names:
				if f'"{component_name}"' in line:
					failures.append(f"{relative}:{line_number}: component content belongs in Lua: {line.strip()}")
	if failures:
		print("weapon execution must branch on validated Lua behavior tags, not content IDs", file=sys.stderr)
		print("\n".join(failures), file=sys.stderr)
		return 1
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
