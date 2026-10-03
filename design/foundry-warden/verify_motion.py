"""Check the EXPORTED motion rig, including reduced keys and visible geometry.

This uses the independent preview renderer's interpolation, not the author's
procedural poses. Run after polish_animation.py from any working directory.
"""

from __future__ import annotations

import json
import math
import re
from pathlib import Path

from PIL import Image
import render_assets as renderer

HERE = Path(__file__).resolve().parent
ASSETS = HERE.parents[1] / "data/anim/foundry_warden"


def convex_hull(points):
    points = sorted(set(points))

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    lower, upper = [], []
    for p in points:
        while len(lower) > 1 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(points):
        while len(upper) > 1 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def verify():
    renderer.RIG = json.loads(
        (ASSETS / "warden-motion-v2.json").read_text(encoding="utf-8")
    )
    meta = json.loads(
        (ASSETS / "warden-motion-v2.combat.json").read_text(encoding="utf-8")
    )
    atlas = Image.open(ASSETS / "warden-motion-v2.png").convert("RGBA")
    rects = {
        m[1]: tuple(map(int, m.groups()[1:]))
        for m in re.finditer(
            r"\n([^\n ]+)\n  rotate: false\n  xy: (\d+), (\d+)\n  size: (\d+), (\d+)",
            (ASSETS / "warden-motion-v2.atlas").read_text(),
        )
    }
    hulls = {}
    for name, (x, y, w, h) in rects.items():
        assert x >= 0 and y >= 0 and x + w <= atlas.width and y + h <= atlas.height
        alpha = atlas.crop((x, y, x + w, y + h)).getchannel("A")
        points = []
        for py in range(h):
            xs = [px for px in range(w) if alpha.getpixel((px, py)) > 128]
            if xs:
                points.extend([(xs[0], py), (xs[-1], py)])
        assert points, name
        hulls[name] = convex_hull(points)
    metrics = {}
    for clip, info in meta["clips"].items():
        duration = info["duration"]
        for name, tracks in renderer.RIG["animations"][clip]["bones"].items():
            for channel, keys in tracks.items():
                times = [key["time"] for key in keys]
                assert times == sorted(set(times)), (
                    clip,
                    name,
                    "duplicate or unordered keys",
                )
                assert times[0] == 0 and abs(times[-1] - duration) < 0.00001, (
                    clip,
                    name,
                    "short timeline",
                )
                assert all(math.isfinite(v) for key in keys for v in key.values())
                if channel == "rotate":
                    assert all(
                        abs(b["angle"] - a["angle"]) < 75
                        for a, b in zip(keys, keys[1:])
                    ), (clip, name, "ambiguous angular interpolation")
        lowest, highest, foot_error, loop_error = 999.0, -999.0, 0.0, 0.0
        lowest_part = ""
        for k in range(121):
            time = duration * k / 120
            world = renderer.pose(clip, time)
            assert all(
                all(math.isfinite(x) for x in matrix) for matrix in world.values()
            )
            airborne = clip == "furnace_slam" and 0.52 < time < 1.12
            if not airborne:
                for name in ["rear_l", "rear_r", "front_l", "front_r"]:
                    phase = (
                        time / duration + (0 if name in ("rear_l", "front_r") else 0.5)
                    ) % 1
                    if clip == "walk" and phase >= 0.68:
                        continue
                    foot_error = max(
                        foot_error,
                        abs(world[name + "_toe"][5] - meta["foot_contact_height"]),
                    )
            for slot in renderer.RIG["slots"]:
                a = renderer.RIG["skins"]["default"][slot["name"]][slot["attachment"]]
                m = renderer.mul(
                    world[slot["bone"]],
                    renderer.local(
                        a.get("x", 0),
                        a.get("y", 0),
                        a.get("rotation", 0),
                        a.get("scaleX", 1),
                        a.get("scaleY", 1),
                    ),
                )
                _, _, w, h = rects[slot["attachment"]]
                points = [
                    renderer.point(
                        m, (px / w - 0.5) * a["width"], (0.5 - py / h) * a["height"]
                    )
                    for px, py in hulls[slot["attachment"]]
                ]
                current = min(p[1] for p in points)
                if current < lowest:
                    lowest, lowest_part = current, f"{slot['name']} @ {time:.3f}"
                highest = max(highest, max(p[1] for p in points))
        if info["loop"]:
            start, end = renderer.pose(clip, 0), renderer.pose(clip, duration)
            loop_error = max(
                abs(start[n][i] - end[n][i]) for n in start for i in range(6)
            )
        assert foot_error < 0.7, (clip, "support foot drift", foot_error)
        assert loop_error < 0.001, (clip, "loop seam", loop_error)
        assert lowest >= -0.85, (
            clip,
            "visible geometry below ground",
            lowest_part,
            lowest,
        )
        assert highest < 433, (clip, "preview top clipping", highest)
        metrics[clip] = dict(
            exported_foot_error=round(foot_error, 4),
            lowest_visible_y=round(lowest, 3),
            highest_visible_y=round(highest, 3),
            lowest_part=lowest_part,
            loop_matrix_error=round(loop_error, 6),
        )
    report = HERE / "motion-validation.json"
    data = json.loads(report.read_text())
    data["exported_pose_checks"] = metrics
    report.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(
        f"PASS: {len(metrics)} exported animations, 121 poses per clip, intact loop seams and planted feet"
    )
    print(
        f"Maximum exported support-foot error: {max(m['exported_foot_error'] for m in metrics.values()):.4f} world units"
    )
    print(
        f"Lowest visible point: {min(m['lowest_visible_y'] for m in metrics.values()):.3f} world units"
    )
    return metrics


if __name__ == "__main__":
    verify()
