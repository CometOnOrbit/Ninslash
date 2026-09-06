#!/usr/bin/env python3
"""Keep LuaLS weapon declarations synchronized with the native API."""

import pathlib
import re
import sys


def fields(source: str, class_name: str, next_class: str) -> set[str]:
	start = source.index(f"---@class {class_name}")
	end = source.index(f"---@class {next_class}", start)
	return set(re.findall(r"^---@field (\w+)\?? ", source[start:end], re.MULTILINE))


def report(name: str, expected: set[str], declared: set[str], failures: list[str]) -> None:
	missing = sorted(expected - declared)
	if missing:
		failures.append(f"{name} declarations missing: {', '.join(missing)}")


def main() -> int:
	root = pathlib.Path(__file__).resolve().parents[1]
	api = (root / "data/weapons/ninslash_api.lua").read_text(encoding="utf-8")
	loader = (root / "src/game/weapons/weapon_lua.cpp").read_text(encoding="utf-8")
	runtime = (root / "src/game/weapons/weapon_script_runtime.cpp").read_text(encoding="utf-8")
	presentation = (root / "src/game/weapons/weapon_presentation_runtime.cpp").read_text(encoding="utf-8")
	failures: list[str] = []

	combat = set(re.findall(r'COMBAT_(?:INT|FLOAT|BOOL)\("([^"]+)"', loader))
	visual = set(re.findall(r'VISUAL_(?:INT|FLOAT|VEC_INT|VEC_FLOAT)\("([^"]+)"', loader))
	report("combat field", combat, fields(api, "WeaponCombat", "WeaponVisuals"), failures)
	report("visual field", visual, fields(api, "WeaponVisuals", "WeaponAssets"), failures)

	event_block = runtime[runtime.index("const char *apEvents"):runtime.index('lua_setglobal(gs_pState, "weapon")')]
	events = set(re.findall(r'"(on_[a-z]+)"', event_block))
	declared_events = set(re.findall(r"^function weapon\.(on_[a-z]+)", api, re.MULTILINE))
	report("lifecycle event", events, declared_events, failures)

	runtime_context = {"state_get", "state_set", "random", "visual"}
	runtime_context.update(re.findall(r'\? "(spawn_[a-z]+)"', runtime))
	command_block = runtime[runtime.index("aCommands[]"):runtime.index("for(const auto &Command", runtime.index("aCommands[]"))]
	runtime_context.update(re.findall(r'\{"([a-z_]+)"', command_block))
	declared_context = set(re.findall(r"^function WeaponContext:(\w+)", api, re.MULTILINE))
	report("combat context method", runtime_context, declared_context, failures)

	presentation_context = set(re.findall(r'lua_setfield\(gs_pState, -2, "([a-z_]+)"\);', presentation))
	presentation_context.discard("on_hud")
	declared_presentation = set(re.findall(r"^function WeaponPresentationContext:(\w+)", api, re.MULTILINE))
	report("presentation context method", presentation_context, declared_presentation, failures)

	if failures:
		print("LuaLS weapon API declarations are out of sync with C++", file=sys.stderr)
		print("\n".join(failures), file=sys.stderr)
		return 1
	return 0


if __name__ == "__main__":
	raise SystemExit(main())

