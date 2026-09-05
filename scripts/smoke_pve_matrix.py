#!/usr/bin/env python3
"""Run a fixed-seed dedicated-server smoke matrix for all three PvE modes."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MODES = ("invasion", "horde", "extract")
MODE_CONFIGS = {
    "invasion": "invasion1.cfg",
    "horde": "horde_foundry.cfg",
    "extract": "extract_foundry.cfg",
}
DEFAULT_EXECUTABLES = (
    ROOT / "build-win" / "Release" / "ninslash_srv.exe",
    ROOT / "build-win-verify" / "Release" / "ninslash_srv.exe",
    ROOT / "build" / "ninslash_srv",
)
SUCCESS_MARKERS = ("generated.map", "waypoints", "connections")
FAILURE_MARKERS = ("failed to load", "failed to open", "assertion failed", "runtime error", "exception")


def find_executable(explicit: str | None) -> Path:
    if explicit:
        path = Path(explicit).expanduser().resolve()
        if not path.is_file():
            raise FileNotFoundError(f"server executable does not exist: {path}")
        return path
    for path in DEFAULT_EXECUTABLES:
        if path.is_file():
            return path
    searched = "\n  ".join(str(path) for path in DEFAULT_EXECUTABLES)
    raise FileNotFoundError(
        "ninslash dedicated server executable was not found. Build Release first or "
        f"pass --server PATH. Searched:\n  {searched}"
    )


def run_case(server: Path, mode: str, seed: int, depth: int, timeout: float, log_dir: Path, port: int | None) -> tuple[bool, str]:
    config = ROOT / "cfg" / MODE_CONFIGS[mode]
    if not config.is_file():
        return False, f"missing config: {config}"
    log_dir.mkdir(parents=True, exist_ok=True)
    log_path = log_dir / f"{mode}-depth-{depth}-seed-{seed}.log"
    command = [str(server), "-f", str(config), f"sv_mapgen_seed {seed}", f"sv_mapgen_level {depth}", "sv_mapgen_random_seed 0"]
    if port is not None:
        command.append(f"sv_port {port}")
    started = time.monotonic()
    creationflags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    process = subprocess.Popen(
        command,
        cwd=ROOT,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        creationflags=creationflags,
    )
    timed_out = False
    try:
        output, _ = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out = True
        process.terminate()
        try:
            output, _ = process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            output, _ = process.communicate()
    elapsed = time.monotonic() - started
    log_path.write_text(output, encoding="utf-8")
    lowered = output.lower()
    failures = [marker for marker in FAILURE_MARKERS if marker in lowered]
    successes = [marker for marker in SUCCESS_MARKERS if marker in lowered]
    # A healthy dedicated server normally remains alive, so reaching the timeout is
    # expected. An early non-zero exit is not.
    healthy_exit = timed_out or process.returncode == 0
    ok = healthy_exit and not failures and len(successes) >= 2
    detail = f"{mode} depth={depth} seed={seed}: {'PASS' if ok else 'FAIL'} ({elapsed:.1f}s, log={log_path})"
    if not healthy_exit:
        detail += f"; exited with {process.returncode}"
    if failures:
        detail += f"; failure markers={failures}"
    if len(successes) < 2:
        detail += f"; generation markers found={successes}"
    return ok, detail


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server", help="path to ninslash_srv(.exe); auto-detected when omitted")
    parser.add_argument("--seed", type=int, action="append", dest="seeds", help="fixed seed; repeat for a larger matrix")
    parser.add_argument("--depth", type=int, action="append", dest="depths", help="map depth; repeat for a larger matrix (default: 1,5,10,15)")
    parser.add_argument("--timeout", type=float, default=15.0, help="seconds per case (default: 15)")
    parser.add_argument("--port", type=int, help="dedicated server port; use this when the default port is occupied")
    parser.add_argument("--log-dir", default=str(ROOT / "smoke-logs"), help="output directory for server logs")
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    if args.port is not None and not 1 <= args.port <= 65535:
        parser.error("--port must be in range 1..65535")
    try:
        server = find_executable(args.server)
    except FileNotFoundError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 2
    seeds = args.seeds or [1337]
    depths = args.depths or [1, 5, 10, 15]
    print(f"Server: {server}")
    results = [run_case(server, mode, seed, depth, args.timeout, Path(args.log_dir), args.port) for seed in seeds for depth in depths for mode in MODES]
    for _, detail in results:
        print(detail)
    passed = sum(ok for ok, _ in results)
    print(f"Matrix: {passed}/{len(results)} passed ({len(seeds)} fixed seed(s) x {len(depths)} depths x {len(MODES)} modes)")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
