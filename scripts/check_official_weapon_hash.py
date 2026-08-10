#!/usr/bin/env python3
"""Verify the runtime official hash against the exact source embedding rules."""

import hashlib
import pathlib
import subprocess
import sys

from embed_binary import canonicalize_lua_bytes


def main() -> int:
	if len(sys.argv) != 2:
		raise SystemExit("usage: check_official_weapon_hash.py <hash-executable>")
	root = pathlib.Path(__file__).resolve().parents[1]
	dsl = canonicalize_lua_bytes((root / "data/weapons/weapon_dsl.lua").read_bytes())
	manifest = root / "data/weapons/official_manifest.txt"
	chunks = []
	seen = set()
	for raw_line in manifest.read_text(encoding="utf-8").splitlines():
		line = raw_line.strip()
		if not line or line.startswith("#"):
			continue
		path = manifest.parent / line
		if path in seen or not path.is_file():
			raise SystemExit(f"invalid official manifest entry: {line}")
		seen.add(path)
		data = canonicalize_lua_bytes(path.read_bytes())
		chunks.append(b"-- source: " + line.encode("utf-8") + b"\n" + data)
	embedded = b"\n".join(chunks)
	expected = hashlib.sha256(dsl + embedded).hexdigest()
	actual = subprocess.check_output([sys.argv[1]], text=True).strip()
	if actual != expected:
		print(f"official weapon hash mismatch: runtime={actual} source={expected}", file=sys.stderr)
		return 1
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
