#!/usr/bin/env python3
"""Check that project headers have valid, unique include guards."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parent.parent
SOURCE_DIR = ROOT / "src"
SKIPPED_DIRS = {"external", "generated"}
SKIPPED_FILES = {"keynames.h"}


def check_file(filename: Path) -> tuple[str | None, str | None]:
	contents = filename.read_text(encoding="utf-8")
	if re.search(r"^\s*#\s*pragma\s+once\s*$", contents, re.MULTILINE):
		return None, None
	ifndef = re.search(r"^\s*#\s*ifndef\s+([A-Za-z_]\w*)\s*$", contents, re.MULTILINE)
	if not ifndef:
		return f"Missing header guard in {filename.relative_to(ROOT)}", None
	guard = ifndef.group(1)
	define = re.search(rf"^\s*#\s*define\s+{re.escape(guard)}(?:\s|$)", contents, re.MULTILINE)
	if not define:
		return f"Header guard {guard} is not defined in {filename.relative_to(ROOT)}", guard
	return None, guard


def main() -> int:
	errors = []
	guards: dict[str, Path] = {}
	for filename in sorted(SOURCE_DIR.rglob("*.h")):
		if filename.name in SKIPPED_FILES or any(part in SKIPPED_DIRS for part in filename.relative_to(SOURCE_DIR).parts):
			continue
		error, guard = check_file(filename)
		if error:
			errors.append(error)
		if guard in guards:
			errors.append(
				f"Duplicate header guard {guard} in {filename.relative_to(ROOT)} and {guards[guard].relative_to(ROOT)}"
			)
		elif guard:
			guards[guard] = filename
	for error in errors:
		print(error)
	if errors:
		print(f"FAIL: {len(errors)} header guard error(s)")
		return 1
	print("OK: header guards")
	return 0


if __name__ == "__main__":
	raise SystemExit(main())
