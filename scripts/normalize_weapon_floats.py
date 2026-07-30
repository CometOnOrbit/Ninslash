#!/usr/bin/env python3
"""Normalize decimal literals in official weapon Lua without binary-float noise."""

import argparse
from decimal import Decimal, ROUND_HALF_UP
import pathlib
import re

from weapon_lua_files import official_weapon_paths


DECIMAL_LITERAL = re.compile(r"(?<![\w.])-?\d+\.\d+(?![\w.])")
SIGNIFICANT_DIGITS = 7


def normalize(match: re.Match[str]) -> str:
	value = Decimal(match.group(0))
	if value:
		quantum = Decimal(1).scaleb(value.copy_abs().adjusted() - SIGNIFICANT_DIGITS + 1)
		value = value.quantize(quantum, rounding=ROUND_HALF_UP)
	text = format(value, "f").rstrip("0").rstrip(".")
	if text == "-0":
		return "0"
	return text


def main() -> int:
	parser = argparse.ArgumentParser()
	parser.add_argument("--write", action="store_true")
	args = parser.parse_args()
	root = pathlib.Path(__file__).resolve().parents[1]
	paths = official_weapon_paths(root)
	dirty = []
	for path in paths:
		text = path.read_text(encoding="utf-8")
		normalized = DECIMAL_LITERAL.sub(normalize, text)
		if normalized != text:
			dirty.append(path.relative_to(root))
			if args.write:
				path.write_text(normalized, encoding="utf-8")
	if dirty and not args.write:
		for path in dirty:
			print(path)
		return 1
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
