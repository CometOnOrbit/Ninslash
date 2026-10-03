"""Evaluate and render the exported Foundry Warden region rig (Pillow only)."""

from __future__ import annotations

import json
import math
import re
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
ASSETS = ROOT / "data/anim/foundry_warden"
RIG = json.loads((ASSETS / "warden-motion-v2.json").read_text(encoding="utf-8"))
META = json.loads((ASSETS / "warden-motion-v2.combat.json").read_text(encoding="utf-8"))
ATLAS = Image.open(ASSETS / "warden-motion-v2.png").convert("RGBA")
REGIONS = {}
for match in re.finditer(
    r"\n([^\n ]+)\n  rotate: false\n  xy: (\d+), (\d+)\n  size: (\d+), (\d+)",
    (ASSETS / "warden-motion-v2.atlas").read_text(),
):
    name = match[1]
    x, y, w, h = map(int, match.groups()[1:])
    REGIONS[name] = ATLAS.crop((x, y, x + w, y + h))


def mul(a, b):
    return (
        a[0] * b[0] + a[2] * b[1],
        a[1] * b[0] + a[3] * b[1],
        a[0] * b[2] + a[2] * b[3],
        a[1] * b[2] + a[3] * b[3],
        a[0] * b[4] + a[2] * b[5] + a[4],
        a[1] * b[4] + a[3] * b[5] + a[5],
    )


def local(x, y, r=0, sx=1, sy=1):
    angle = math.radians(r)
    c, s = math.cos(angle), math.sin(angle)
    return (c * sx, s * sx, -s * sy, c * sy, x, y)


def sample(keys, t, kind):
    default = (1, 1) if kind == "scale" else (0, 0)
    if not keys:
        return default
    a = b = keys[0]
    for b in keys[1:]:
        if t <= b["time"]:
            break
        a = b
    f = (
        max(0, min(1, (t - a["time"]) / (b["time"] - a["time"])))
        if b["time"] > a["time"]
        else 0
    )
    if a.get("curve") == "stepped":
        f = 0
    if kind == "rotate":
        return (a.get("angle", 0) + (b.get("angle", 0) - a.get("angle", 0)) * f, 0)
    return tuple(
        a.get(k, d) + (b.get(k, d) - a.get(k, d)) * f
        for k, d in zip(("x", "y"), default)
    )


def pose(clip, t):
    out = {}
    tracks = RIG["animations"][clip]["bones"]
    for b in RIG["bones"]:
        tr = tracks.get(b["name"], {})
        p = sample(tr.get("translate"), t, "translate")
        r = sample(tr.get("rotate"), t, "rotate")[0]
        s = sample(tr.get("scale"), t, "scale")
        m = local(
            b.get("x", 0) + p[0],
            b.get("y", 0) + p[1],
            b.get("rotation", 0) + r,
            b.get("scaleX", 1) * s[0],
            b.get("scaleY", 1) * s[1],
        )
        out[b["name"]] = mul(out[b["parent"]], m) if "parent" in b else m
    return out


def point(m, x, y):
    return (m[0] * x + m[2] * y + m[4], m[1] * x + m[3] * y + m[5])


def render(
    clip="idle",
    time=0,
    size=(1100, 650),
    scale=1.5,
    origin=None,
    bones=False,
    background=None,
):
    image = Image.new("RGBA", size, background or (0, 0, 0, 0))
    origin = origin or (size[0] / 2, size[1] - 75)
    camera = (scale, 0, 0, -scale, *origin)
    world = pose(clip, time)
    for slot in RIG["slots"]:
        a = RIG["skins"]["default"][slot["name"]][slot["attachment"]]
        sprite = REGIONS[slot["attachment"]]
        attachment = local(
            a.get("x", 0),
            a.get("y", 0),
            a.get("rotation", 0),
            a.get("scaleX", 1),
            a.get("scaleY", 1),
        )
        pixel = (
            a["width"] / sprite.width,
            0,
            0,
            -a["height"] / sprite.height,
            -a["width"] / 2,
            a["height"] / 2,
        )
        m = mul(mul(mul(camera, world[slot["bone"]]), attachment), pixel)
        corners = [
            point(m, x, y)
            for x, y in [
                (0, 0),
                (sprite.width, 0),
                (sprite.width, sprite.height),
                (0, sprite.height),
            ]
        ]
        left = max(0, math.floor(min(p[0] for p in corners)) - 2)
        top = max(0, math.floor(min(p[1] for p in corners)) - 2)
        right = min(size[0], math.ceil(max(p[0] for p in corners)) + 2)
        bottom = min(size[1], math.ceil(max(p[1] for p in corners)) + 2)
        if right <= left or bottom <= top:
            continue
        a0, b, c, d, x, y = m
        det = a0 * d - b * c
        inverse = (
            d / det,
            -c / det,
            (d * (left - x) - c * (top - y)) / det,
            -b / det,
            a0 / det,
            (-b * (left - x) + a0 * (top - y)) / det,
        )
        warped = sprite.transform(
            (right - left, bottom - top),
            Image.Transform.AFFINE,
            inverse,
            resample=Image.Resampling.BICUBIC,
        )
        image.alpha_composite(warped, (left, top))
    if bones:
        draw = ImageDraw.Draw(image)
        for b in RIG["bones"]:
            end = point(camera, *world[b["name"]][4:])
            if "parent" in b:
                start = point(camera, *world[b["parent"]][4:])
                draw.line([start, end], fill=(105, 237, 230, 220), width=2)
            x, y = end
            draw.ellipse(
                (x - 3, y - 3, x + 3, y + 3), fill=(91, 239, 231), outline=(20, 37, 39)
            )
    return image


def font(size, bold=False):
    for path in (
        ["C:/Windows/Fonts/msyhbd.ttc", "C:/Windows/Fonts/arialbd.ttf"]
        if bold
        else ["C:/Windows/Fonts/msyh.ttc", "C:/Windows/Fonts/arial.ttf"]
    ):
        if Path(path).exists():
            return ImageFont.truetype(path, size)
    return ImageFont.load_default(size=size)


def stage(size):
    w, h = size
    im = Image.new("RGBA", size, (26, 34, 38))
    d = ImageDraw.Draw(im)
    for y in range(h):
        a = y / h
        col = (int(23 + 12 * a), int(32 + 15 * a), int(37 + 14 * a), 255)
        d.line((0, y, w, y), fill=col)
    for x in range(70, w, 190):
        d.line((x, 80, x, h - 65), fill=(28, 39, 43), width=12)
        d.line((x, 120, x + 160, 290, x, 445), fill=(31, 43, 47), width=8)
    d.rectangle((0, h - 64, w, h), fill=(16, 24, 27))
    d.line((0, h - 65, w, h - 65), fill=(86, 103, 99), width=2)
    for x in range(10, w, 66):
        d.line((x, h - 59, x + 25, h - 59), fill=(119, 109, 78), width=2)
    return im


def outputs():
    render(time=0.7).save(HERE / "motion-v2-hero.png", optimize=True)
    sheet = Image.new("RGB", (1600, 680), (23, 32, 36))
    draw = ImageDraw.Draw(sheet)
    draw.text(
        (40, 25), "FOUNDRY WARDEN / MOTION PASS 02", font=font(25, True), fill="#e4c180"
    )
    poses = [
        ("clamp_sweep", 0.59, "01 / WINDUP"),
        ("furnace_slam", 1.20, "02 / LANDING"),
        ("vent", 1.4, "03 / VENTING"),
    ]
    for index, (clip, time, label) in enumerate(poses):
        image = render(
            clip, time, size=(520, 495), scale=0.70, origin=(260, 435), bones=index == 1
        )
        sheet.paste(image, (index * 533 + 6, 80), image)
        draw.line(
            (index * 533 + 24, 516, index * 533 + 505, 516), fill="#607975", width=1
        )
        draw.text((index * 533 + 35, 570), label, font=font(20), fill="#dfe4d7")
    draw.text(
        (40, 632),
        "54 bones / 9 clips / baked foot constraints / native region assets",
        font=font(17),
        fill="#9fb3af",
    )
    sheet.save(HERE / "motion-v2-keyposes.png", optimize=True)
    print("Rendered motion-v2-hero.png and motion-v2-keyposes.png")


if __name__ == "__main__":
    outputs()
