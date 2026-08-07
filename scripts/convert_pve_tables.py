#!/usr/bin/env python3
"""Extract PvE card/contract definitions from pve_roguelite.{h,cpp} into JSON.

The runtime loads these JSON files (embedded at build time) instead of the
hardcoded C++ tables, so new cards/contracts no longer require recompiling the
game. Run from the repository root after changing pve_roguelite.cpp:

    python3 scripts/convert_pve_tables.py
"""

import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
H = ROOT / "src/game/pve_roguelite.h"
CPP = ROOT / "src/game/pve_roguelite.cpp"
OUT_DIR = ROOT / "data/pve"


def parse_enums(text):
	"""Map enum member names to values for the enums we care about."""
	enums = {}
	for name, body in re.findall(r"enum\s+(EPve\w+)\s*\{(.*?)\n\};", text, re.S):
		values = {}
		next_val = 0
		for line in body.splitlines():
			line = line.split("//")[0].strip()
			if not line:
				continue
			for member in line.split(","):
				member = member.strip()
				if not member:
					continue
				if "=" in member:
					key, _, val = member.partition("=")
					val = val.strip()
					if val.startswith("1 <<"):
						next_val = 1 << int(val.split("<<")[1].strip())
					else:
						try:
							next_val = int(val, 0)
						except ValueError:
							# References other constants (e.g. NUM_PVE_CARDS);
							# not needed for the card/contract tables.
							continue
				else:
					key = member
				values[key.strip()] = next_val
				next_val += 1
		enums[name] = values
	return enums


def split_args(text):
	"""Split macro/initializer args on top-level commas (strings may contain commas)."""
	args, depth, cur, in_str, esc = [], 0, "", False, False
	for ch in text:
		if in_str:
			cur += ch
			if esc:
				esc = False
			elif ch == "\\":
				esc = True
			elif ch == '"':
				in_str = False
			continue
		if ch == '"':
			in_str = True
			cur += ch
		elif ch in "([{":
			depth += 1
			cur += ch
		elif ch in ")]}":
			depth -= 1
			cur += ch
		elif ch == "," and depth == 0:
			args.append(cur.strip())
			cur = ""
		else:
			cur += ch
	if cur.strip():
		args.append(cur.strip())
	return args


def unquote(s):
	s = s.strip()
	if s.startswith('"') and s.endswith('"'):
		return json.loads(s)
	return s


def eval_arg(arg, enums):
	arg = arg.strip()
	if arg in ("true", "false"):
		return arg == "true"
	if " | " in arg or "|" in arg:
		value = 0
		for part in re.split(r"\s*\|\s*", arg):
			value |= eval_arg(part, enums)
		return value
	m = re.fullmatch(r"(PVE_[A-Z0-9_]+)", arg)
	if m:
		for enum in enums.values():
			if arg in enum:
				return enum[arg]
		raise ValueError(f"unknown enum member {arg}")
	return int(arg, 0)


def extract_block(text, marker):
	start = text.index(marker)
	open_idx = text.index("{", start)
	depth = 0
	i = open_idx
	while i < len(text):
		if text[i] == "{":
			depth += 1
		elif text[i] == "}":
			depth -= 1
			if depth == 0:
				return text[open_idx + 1 : i]
		i += 1
	raise ValueError(f"unbalanced block for {marker}")


def main():
	enums = parse_enums(H.read_text(encoding="utf-8"))
	card_enum = enums.get("EPveCard", {})
	cpp = CPP.read_text(encoding="utf-8")

	# Short descriptions (index == card id).
	short_block = extract_block(cpp, "gs_apShortCardDescriptions")
	shorts = [unquote(s) for s in re.findall(r'"((?:[^"\\]|\\.)*)"', short_block)]

	# Cards via CARD0/CARD1/CARD3 macro calls.
	cards_block = extract_block(cpp, "gs_aCards")
	cards = []
	for m in re.finditer(r"CARD([013])\s*\(", cards_block):
		arity = int(m.group(1))
		body_start = m.end()
		depth, i = 1, body_start
		while i < len(cards_block) and depth:
			if cards_block[i] == "(":
				depth += 1
			elif cards_block[i] == ")":
				depth -= 1
			i += 1
		body = cards_block[body_start : i - 1]
		args = split_args(body)
		expected = {0: 14, 1: 15, 3: 17}[arity]
		if len(args) != expected:
			raise ValueError(f"CARD{arity} arg count {len(args)} != {expected}: {args[0]}")
		(iden, name, desc, rarity, stacks, cost, base, legendary, keywords, *rest) = args
		prereqs = []
		num_prereqs = 0
		tab = branch = tier = mode = spec = None
		if arity == 0:
			tab, branch, tier, mode, spec = rest
		elif arity == 1:
			prereqs = [eval_arg(unquote(rest[0]), enums)]
			num_prereqs = 1
			tab, branch, tier, mode, spec = rest[1:]
		else:
			prereqs = [eval_arg(unquote(r), enums) for r in rest[:3]]
			num_prereqs = 3
			tab, branch, tier, mode, spec = rest[3:]
		cards.append(
			{
				"id": eval_arg(iden, enums),
				"name": unquote(name),
				"description": unquote(desc),
				"short_description": shorts[len(cards)],
				"rarity": eval_arg(rarity, enums),
				"max_stacks": eval_arg(stacks, enums),
				"research_cost": eval_arg(cost, enums),
				"base": eval_arg(base, enums),
				"legendary": eval_arg(legendary, enums),
				"keywords": eval_arg(keywords, enums),
				"prerequisites": prereqs,
				"num_prerequisites": num_prereqs,
				"tab": eval_arg(tab, enums),
				"branch": eval_arg(branch, enums),
				"tier": eval_arg(tier, enums),
				"mode": eval_arg(mode, enums),
				"specialization": eval_arg(spec, enums),
			}
		)

	# Contracts: { ENUM, "name", "rule", "risk", MODE } blocks.
	contracts_block = extract_block(cpp, "gs_aContracts")
	contracts = []
	i = 0
	while i < len(contracts_block):
		if contracts_block[i] == "{":
			depth, j = 1, i + 1
			while j < len(contracts_block) and depth:
				if contracts_block[j] == "{":
					depth += 1
				elif contracts_block[j] == "}":
					depth -= 1
				j += 1
			args = split_args(contracts_block[i + 1 : j - 1])
			if len(args) != 5:
				raise ValueError(f"contract arg count {len(args)} != 5")
			contracts.append(
				{
					"id": eval_arg(args[0], enums),
					"name": unquote(args[1]),
					"rule": unquote(args[2]),
					"risk": unquote(args[3]),
					"mode": eval_arg(args[4], enums),
				}
			)
			i = j
		else:
			i += 1

	if len(shorts) != 100 or len(cards) != 100 or len(contracts) != 20:
		raise SystemExit(
			f"unexpected counts: shorts={len(shorts)} cards={len(cards)} contracts={len(contracts)}")
	for idx, card in enumerate(cards):
		if card["id"] != idx:
			raise SystemExit(f"card id mismatch at {idx}: {card['id']}")
	for idx, contract in enumerate(contracts):
		if contract["id"] != idx:
			raise SystemExit(f"contract id mismatch at {idx}: {contract['id']}")

	OUT_DIR.mkdir(parents=True, exist_ok=True)
	(OUT_DIR / "pve_cards.json").write_text(
		json.dumps(cards, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
	(OUT_DIR / "pve_contracts.json").write_text(
		json.dumps(contracts, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
	print(f"OK: {len(cards)} cards, {len(contracts)} contracts, {len(shorts)} short descriptions")


if __name__ == "__main__":
	main()
	sys.exit(0)
