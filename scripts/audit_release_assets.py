#!/usr/bin/env python3
"""Check that every shipped data asset has an explicit release license status."""

import argparse
import csv
import fnmatch
from pathlib import Path


VALID_STATUSES = {"approved", "review_required", "rejected"}


def load_rules(path: Path):
    with path.open(encoding="utf-8", newline="") as handle:
        rules = list(csv.DictReader(handle))
    required = {"pattern", "status", "license", "source", "author", "notes"}
    if not rules or not required.issubset(rules[0]):
        raise ValueError(f"{path} must contain columns: {', '.join(sorted(required))}")
    for line, rule in enumerate(rules, 2):
        if rule["status"] not in VALID_STATUSES:
            raise ValueError(f"{path}:{line}: invalid status {rule['status']!r}")
    return rules


def matching_rule(relative: str, rules):
    result = None
    for rule in rules:
        if fnmatch.fnmatchcase(relative, rule["pattern"]):
            result = rule
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", default="data/asset_licenses.csv")
    parser.add_argument("--data-dir", default="data")
    parser.add_argument("--strict", action="store_true", help="fail unless every asset is approved")
    args = parser.parse_args()

    manifest = Path(args.manifest)
    data_dir = Path(args.data_dir)
    rules = load_rules(manifest)
    assets = sorted(path for path in data_dir.rglob("*") if path.is_file())
    counts = {status: 0 for status in VALID_STATUSES}
    uncovered = []
    blockers = []

    for asset in assets:
        relative = asset.as_posix()
        rule = matching_rule(relative, rules)
        if rule is None:
            uncovered.append(relative)
            continue
        counts[rule["status"]] += 1
        if rule["status"] != "approved":
            blockers.append((relative, rule["status"], rule["notes"]))

    print(
        f"Asset audit: {len(assets)} files; approved={counts['approved']}; "
        f"review_required={counts['review_required']}; rejected={counts['rejected']}; "
        f"uncovered={len(uncovered)}"
    )
    for relative in uncovered[:50]:
        print(f"UNCOVERED {relative}")
    for relative, status, notes in blockers[:50]:
        print(f"{status.upper()} {relative}: {notes}")
    hidden = len(uncovered) + len(blockers) - min(50, len(uncovered)) - min(50, len(blockers))
    if hidden > 0:
        print(f"... {hidden} additional blockers omitted")

    if uncovered:
        return 1
    if args.strict and blockers:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
