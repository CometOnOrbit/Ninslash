#!/usr/bin/env python3
"""Compute or update the canonical Ninslash content package hash."""

import argparse
import hashlib
import json
import pathlib


CONTENT_TYPES = {"mod": 0, "map": 1, "room_preset": 2, "challenge": 3}
CAPABILITIES = {"resources": 1, "client_theme": 2, "gameplay_rules": 4, "weapons": 8, "items": 16,
	"weapon_modules": 32, "forge_recipes": 64}
FILE_GROUPS = (("maps", 0), ("resources", 1), ("scripts", 2), ("definitions", 3))


def add_text(digest, value):
	digest.update(str(value).encode("utf-8") + b"\0")


def package_hash(root, manifest):
	digest = hashlib.sha256()
	add_text(digest, manifest["schema_version"])
	content_type = manifest["content_type"]
	if content_type not in CONTENT_TYPES:
		raise ValueError("unknown content_type")
	add_text(digest, content_type)
	for key in ("published_file_id", "name", "description", "author", "version", "target_protocol", "content_rating"):
		add_text(digest, manifest[key])
	capability_mask = 0
	for capability in manifest.get("capabilities", []):
		if capability not in CAPABILITIES or capability_mask & CAPABILITIES[capability]:
			raise ValueError("unknown or duplicate capability: " + capability)
		capability_mask |= CAPABILITIES[capability]
	add_text(digest, "%d:%d" % (manifest.get("api_version", 0), capability_mask))
	for dependency in sorted(manifest.get("dependencies", []), key=lambda item: item["published_file_id"]):
		for key in ("published_file_id", "version", "content_hash"):
			add_text(digest, dependency[key])
	files = []
	for group, file_type in FILE_GROUPS:
		files.extend((path, file_type) for path in manifest.get(group, []))
	for relative, file_type in sorted(files):
		path = root / relative
		data = path.read_bytes()
		add_text(digest, relative)
		add_text(digest, file_type)
		digest.update(len(data).to_bytes(8, "big"))
		digest.update(data)
	return digest.hexdigest()


def main():
	parser = argparse.ArgumentParser()
	parser.add_argument("package", type=pathlib.Path)
	parser.add_argument("--write", action="store_true")
	args = parser.parse_args()
	manifest_path = args.package / "ninslash_content.json"
	manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
	value = package_hash(args.package, manifest)
	if args.write:
		manifest["content_hash"] = value
		manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
	print(value)


if __name__ == "__main__":
	main()
