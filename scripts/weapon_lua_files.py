"""Canonical official weapon source list."""

from pathlib import Path


def official_weapon_paths(root: Path) -> list[Path]:
	manifest = root / "data" / "weapons" / "official_manifest.txt"
	paths = []
	seen = set()
	for raw_line in manifest.read_text(encoding="utf-8").splitlines():
		line = raw_line.strip()
		if not line or line.startswith("#"):
			continue
		path = manifest.parent / line
		if ".." in Path(line).parts or path in seen or not path.is_file() or path.suffix != ".lua":
			raise ValueError(f"invalid official weapon manifest entry: {line}")
		seen.add(path)
		paths.append(path)
	discovered = set((manifest.parent / "official").rglob("*.lua"))
	if discovered != seen:
		missing = sorted(str(path.relative_to(manifest.parent)) for path in discovered - seen)
		raise ValueError(f"official weapon files missing from manifest: {', '.join(missing)}")
	return paths
