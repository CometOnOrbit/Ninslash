#!/usr/bin/env python3
"""Build the deterministic three-cell PvE cargo atlas."""

from __future__ import annotations

import hashlib
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parent.parent
SCALE = 4
CELL = 64


def sc(value: float) -> int:
    return round(value * SCALE)


def rect(cell: int, box: tuple[float, float, float, float]) -> tuple[int, int, int, int]:
    x0, y0, x1, y1 = box
    offset = cell * CELL
    return sc(offset + x0), sc(y0), sc(offset + x1), sc(y1)


def points(cell: int, values: list[tuple[float, float]]) -> list[tuple[int, int]]:
    offset = cell * CELL
    return [(sc(offset + x), sc(y)) for x, y in values]


def line(draw: ImageDraw.ImageDraw, cell: int, values: list[tuple[float, float]], fill, width: float) -> None:
    draw.line(points(cell, values), fill=fill, width=sc(width), joint="curve")


def build() -> Image.Image:
    image = Image.new("RGBA", (CELL * 3 * SCALE, CELL * SCALE), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image, "RGBA")

    outline = (24, 28, 34, 255)
    deep = (48, 55, 63, 255)
    steel = (116, 127, 136, 255)
    bright = (218, 226, 228, 255)

    # Coolant core: a sealed cryogenic canister with visible coolant and a
    # snowflake/fan mark. Its round capsule silhouette cannot read as the CS bomb.
    c = 0
    draw.rounded_rectangle(rect(c, (13, 9, 51, 56)), radius=sc(10), fill=outline)
    draw.rounded_rectangle(rect(c, (17, 12, 47, 53)), radius=sc(7), fill=steel)
    draw.rectangle(rect(c, (12, 21, 18, 44)), fill=outline)
    draw.rectangle(rect(c, (46, 21, 52, 44)), fill=outline)
    draw.polygon(points(c, [(18, 9), (24, 5), (40, 5), (46, 9)]), fill=outline)
    draw.rounded_rectangle(rect(c, (23, 5, 41, 12)), radius=sc(2), fill=deep)
    draw.polygon(points(c, [(18, 53), (24, 59), (40, 59), (46, 53)]), fill=outline)
    draw.rounded_rectangle(rect(c, (21, 17, 43, 48)), radius=sc(5), fill=(22, 84, 101, 255), outline=outline, width=sc(2))
    draw.rounded_rectangle(rect(c, (24, 21, 40, 45)), radius=sc(4), fill=(57, 208, 220, 255))
    draw.rounded_rectangle(rect(c, (27, 23, 31, 42)), radius=sc(2), fill=(177, 247, 242, 145))
    draw.rectangle(rect(c, (17, 27, 47, 32)), fill=outline)
    draw.rectangle(rect(c, (19, 28, 45, 31)), fill=(82, 94, 103, 255))
    for angle_line in (
        [(32, 34), (32, 42)],
        [(28.5, 36), (35.5, 40)],
        [(35.5, 36), (28.5, 40)],
    ):
        line(draw, c, angle_line, bright, 1.6)
    draw.ellipse(rect(c, (29.5, 35.5, 34.5, 40.5)), fill=(235, 255, 252, 255), outline=outline, width=sc(1))
    line(draw, c, [(20, 14), (24, 11), (40, 11), (44, 14)], (238, 244, 242, 190), 1.3)

    # Data core: a clipped-corner cartridge with a readable circuit lattice.
    c = 1
    shell = [(10, 19), (18, 10), (46, 10), (54, 19), (54, 45), (46, 54), (18, 54), (10, 45)]
    inner = [(15, 21), (21, 15), (43, 15), (49, 21), (49, 43), (43, 49), (21, 49), (15, 43)]
    draw.polygon(points(c, shell), fill=outline)
    draw.polygon(points(c, inner), fill=(54, 64, 77, 255))
    draw.polygon(points(c, [(18, 18), (24, 13), (40, 13), (46, 18), (42, 21), (22, 21)]), fill=(151, 164, 172, 255))
    draw.rounded_rectangle(rect(c, (24, 24, 40, 40)), radius=sc(3), fill=outline)
    draw.rounded_rectangle(rect(c, (27, 27, 37, 37)), radius=sc(2), fill=(75, 220, 232, 255))
    draw.rectangle(rect(c, (30, 30, 34, 34)), fill=(213, 255, 249, 255))
    circuit = (91, 216, 224, 255)
    for values in (
        [(24, 29), (19, 29), (19, 23)],
        [(24, 35), (18, 35), (18, 42), (23, 42)],
        [(40, 29), (45, 29), (45, 22)],
        [(40, 35), (46, 35), (46, 43), (41, 43)],
        [(29, 24), (29, 19)],
        [(35, 40), (35, 46)],
    ):
        line(draw, c, values, circuit, 1.4)
    for x, y in ((19, 23), (23, 42), (45, 22), (41, 43), (29, 19), (35, 46)):
        draw.ellipse(rect(c, (x - 1.5, y - 1.5, x + 1.5, y + 1.5)), fill=bright)
    draw.rectangle(rect(c, (8, 27, 13, 37)), fill=outline)
    draw.rectangle(rect(c, (51, 27, 56, 37)), fill=outline)

    # Energy core: an exposed orange induction ring around a white-hot center.
    c = 2
    draw.ellipse(rect(c, (7, 7, 57, 57)), fill=(255, 112, 18, 35))
    draw.ellipse(rect(c, (10, 10, 54, 54)), fill=outline)
    draw.ellipse(rect(c, (15, 15, 49, 49)), fill=(96, 68, 43, 255))
    draw.ellipse(rect(c, (20, 20, 44, 44)), fill=outline)
    draw.ellipse(rect(c, (24, 24, 40, 40)), fill=(255, 126, 22, 255))
    draw.ellipse(rect(c, (28, 28, 36, 36)), fill=(255, 242, 158, 255))
    for values in (
        [(32, 8), (32, 19)],
        [(32, 45), (32, 56)],
        [(8, 32), (19, 32)],
        [(45, 32), (56, 32)],
        [(15, 15), (23, 23)],
        [(41, 41), (49, 49)],
        [(49, 15), (41, 23)],
        [(23, 41), (15, 49)],
    ):
        line(draw, c, values, outline, 5.5)
        line(draw, c, values, (242, 132, 35, 255), 2.4)
    line(draw, c, [(27, 18), (37, 18)], (255, 212, 105, 255), 1.5)
    line(draw, c, [(27, 46), (37, 46)], (255, 212, 105, 255), 1.5)

    return image.resize((CELL * 3, CELL), Image.Resampling.LANCZOS)


def main() -> None:
    output = ROOT / "data" / "pve_cargo.png"
    image = build()
    image.save(output, optimize=True)
    alpha = image.getchannel("A")
    for cell in range(3):
        crop = alpha.crop((cell * CELL, 0, (cell + 1) * CELL, CELL))
        if crop.getbbox() is None:
            raise ValueError(f"empty PvE cargo cell {cell}")
    digest = hashlib.sha256(output.read_bytes()).hexdigest()[:12]
    print(f"Generated {output.relative_to(ROOT)} ({image.width}x{image.height}, sha256={digest})")


if __name__ == "__main__":
    main()
