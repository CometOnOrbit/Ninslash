#!/usr/bin/env python3
"""Build the deterministic Lost Protocol foundry map theme.

The script intentionally has no third-party dependencies. It draws flat RGBA
PNGs with a tiny software rasterizer and rewrites only the external image names
in the generate_large3 map template. Geometry, tile layers, entities, quads,
envelopes and collision data are copied byte-for-byte.
"""

from __future__ import annotations

import argparse
import hashlib
import math
import struct
import sys
import zlib
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter


Color = tuple[int, int, int, int]

TRANSPARENT: Color = (0, 0, 0, 0)
INK: Color = (21, 25, 30, 255)
STEEL_DARK: Color = (43, 50, 58, 255)
STEEL: Color = (69, 78, 88, 255)
STEEL_LIGHT: Color = (111, 124, 136, 255)
EDGE: Color = (162, 176, 185, 255)
ORANGE: Color = (255, 119, 30, 255)
AMBER: Color = (255, 185, 48, 255)
HEAT: Color = (255, 73, 22, 255)
CYAN: Color = (59, 211, 216, 255)


class Canvas:
    def __init__(self, width: int, height: int, color: Color = TRANSPARENT):
        self.width = width
        self.height = height
        self.pixels = bytearray(color * (width * height))

    def _blend(self, x: int, y: int, color: Color) -> None:
        if x < 0 or y < 0 or x >= self.width or y >= self.height:
            return
        i = (y * self.width + x) * 4
        alpha = color[3]
        if alpha == 255:
            self.pixels[i : i + 4] = bytes(color)
        elif alpha:
            inv = 255 - alpha
            for channel in range(3):
                self.pixels[i + channel] = (
                    color[channel] * alpha + self.pixels[i + channel] * inv
                ) // 255
            self.pixels[i + 3] = min(255, alpha + self.pixels[i + 3] * inv // 255)

    def rect(self, x0: int, y0: int, x1: int, y1: int, color: Color) -> None:
        x0, x1 = max(0, x0), min(self.width, x1)
        y0, y1 = max(0, y0), min(self.height, y1)
        if x0 >= x1 or y0 >= y1:
            return
        if color[3] in (0, 255):
            row = bytes(color) * (x1 - x0)
            for y in range(y0, y1):
                start = (y * self.width + x0) * 4
                self.pixels[start : start + len(row)] = row
            return
        for y in range(y0, y1):
            for x in range(x0, x1):
                self._blend(x, y, color)

    def gradient(self, top: Color, bottom: Color) -> None:
        denominator = max(1, self.height - 1)
        for y in range(self.height):
            color = tuple(
                top[i] + (bottom[i] - top[i]) * y // denominator for i in range(4)
            )
            self.rect(0, y, self.width, y + 1, color)  # type: ignore[arg-type]

    def polygon(self, points: list[tuple[int, int]], color: Color) -> None:
        if len(points) < 3:
            return
        min_y = max(0, min(y for _, y in points))
        max_y = min(self.height - 1, max(y for _, y in points))
        for y in range(min_y, max_y + 1):
            intersections: list[float] = []
            scan_y = y + 0.5
            for index, (x1, y1) in enumerate(points):
                x2, y2 = points[(index + 1) % len(points)]
                if y1 == y2 or not (min(y1, y2) <= scan_y < max(y1, y2)):
                    continue
                intersections.append(x1 + (scan_y - y1) * (x2 - x1) / (y2 - y1))
            intersections.sort()
            for index in range(0, len(intersections) - 1, 2):
                self.rect(
                    math.floor(intersections[index]),
                    y,
                    math.ceil(intersections[index + 1]),
                    y + 1,
                    color,
                )

    def line(
        self, x1: int, y1: int, x2: int, y2: int, color: Color, width: int = 1
    ) -> None:
        dx, dy = x2 - x1, y2 - y1
        length = max(abs(dx), abs(dy), 1)
        radius = max(0, width // 2)
        for step in range(length + 1):
            x = round(x1 + dx * step / length)
            y = round(y1 + dy * step / length)
            self.rect(x - radius, y - radius, x + radius + 1, y + radius + 1, color)

    def ellipse(self, bounds: tuple[int, int, int, int], color: Color) -> None:
        x0, y0, x1, y1 = bounds
        rx, ry = max(1.0, (x1 - x0) / 2), max(1.0, (y1 - y0) / 2)
        cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
        for y in range(max(0, y0), min(self.height, y1)):
            value = 1.0 - ((y + 0.5 - cy) / ry) ** 2
            if value <= 0:
                continue
            half_width = rx * math.sqrt(value)
            self.rect(math.floor(cx - half_width), y, math.ceil(cx + half_width), y + 1, color)

    def frame(
        self, x0: int, y0: int, x1: int, y1: int, color: Color, width: int = 3
    ) -> None:
        self.rect(x0, y0, x1, y0 + width, color)
        self.rect(x0, y1 - width, x1, y1, color)
        self.rect(x0, y0, x0 + width, y1, color)
        self.rect(x1 - width, y0, x1, y1, color)

    def save_png(self, path: Path) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        scanlines = bytearray()
        stride = self.width * 4
        for y in range(self.height):
            scanlines.append(0)
            scanlines.extend(self.pixels[y * stride : (y + 1) * stride])

        def chunk(name: bytes, payload: bytes) -> bytes:
            checksum = zlib.crc32(name)
            checksum = zlib.crc32(payload, checksum) & 0xFFFFFFFF
            return struct.pack(">I", len(payload)) + name + payload + struct.pack(">I", checksum)

        png = bytearray(b"\x89PNG\r\n\x1a\n")
        png.extend(chunk(b"IHDR", struct.pack(">IIBBBBB", self.width, self.height, 8, 6, 0, 0, 0)))
        png.extend(chunk(b"IDAT", zlib.compress(bytes(scanlines), 9)))
        png.extend(chunk(b"IEND", b""))
        path.write_bytes(png)


def load_rgba_png(path: Path) -> tuple[int, int, bytearray]:
    """Load an 8-bit, non-interlaced RGBA PNG using only the standard library."""
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path} is not a PNG")
    position = 8
    width = height = 0
    compressed = bytearray()
    while position < len(data):
        length = struct.unpack_from(">I", data, position)[0]
        chunk_type = data[position + 4 : position + 8]
        payload = data[position + 8 : position + 8 + length]
        position += 12 + length
        if chunk_type == b"IHDR":
            width, height, depth, color_type, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", payload
            )
            if (depth, color_type, compression, filtering, interlace) != (8, 6, 0, 0, 0):
                raise ValueError(f"unsupported PNG format in {path}")
        elif chunk_type == b"IDAT":
            compressed.extend(payload)
        elif chunk_type == b"IEND":
            break
    if width <= 0 or height <= 0:
        raise ValueError(f"missing PNG header in {path}")
    filtered = zlib.decompress(bytes(compressed))
    stride = width * 4
    expected = (stride + 1) * height
    if len(filtered) != expected:
        raise ValueError(f"invalid decompressed PNG size in {path}")
    pixels = bytearray(width * height * 4)

    def paeth(a: int, b: int, c: int) -> int:
        estimate = a + b - c
        da, db, dc = abs(estimate - a), abs(estimate - b), abs(estimate - c)
        return a if da <= db and da <= dc else b if db <= dc else c

    source = 0
    for y in range(height):
        filter_type = filtered[source]
        source += 1
        row_start = y * stride
        previous_start = (y - 1) * stride
        for x in range(stride):
            value = filtered[source]
            source += 1
            left = pixels[row_start + x - 4] if x >= 4 else 0
            up = pixels[previous_start + x] if y else 0
            upper_left = pixels[previous_start + x - 4] if y and x >= 4 else 0
            if filter_type == 1:
                value = (value + left) & 0xFF
            elif filter_type == 2:
                value = (value + up) & 0xFF
            elif filter_type == 3:
                value = (value + ((left + up) // 2)) & 0xFF
            elif filter_type == 4:
                value = (value + paeth(left, up, upper_left)) & 0xFF
            elif filter_type != 0:
                raise ValueError(f"unsupported PNG filter {filter_type} in {path}")
            pixels[row_start + x] = value
    return width, height, pixels


def panel(canvas: Canvas, x: int, y: int, w: int, h: int, hot: bool = False) -> None:
    canvas.rect(x, y, x + w, y + h, INK)
    canvas.rect(x + 3, y + 3, x + w - 3, y + h - 3, STEEL_DARK)
    canvas.rect(x + 6, y + 6, x + w - 6, y + h - 6, STEEL)
    canvas.line(x + 7, y + 7, x + w - 8, y + 7, STEEL_LIGHT, 2)
    canvas.line(x + 8, y + h - 8, x + w - 8, y + h - 8, (35, 41, 48, 255), 2)
    for px, py in ((x + 10, y + 10), (x + w - 11, y + 10), (x + 10, y + h - 11), (x + w - 11, y + h - 11)):
        canvas.ellipse((px - 3, py - 3, px + 4, py + 4), INK)
        canvas.ellipse((px - 1, py - 1, px + 2, py + 2), EDGE)
    if hot:
        canvas.rect(x + 14, y + h - 15, x + w - 14, y + h - 9, HEAT)
        canvas.rect(x + 20, y + h - 14, x + w - 20, y + h - 11, AMBER)


def hazard_stripes(canvas: Canvas, x: int, y: int, w: int, h: int) -> None:
    canvas.rect(x, y, x + w, y + h, AMBER)
    for offset in range(-h, w + h, 22):
        canvas.polygon(
            [(x + offset, y), (x + offset + 10, y), (x + offset - h + 10, y + h), (x + offset - h, y + h)],
            INK,
        )


def build_main_tileset(path: Path) -> None:
    source_path = path.with_name("metal_main.png")
    width, height, source = load_rgba_png(source_path)
    if (width, height) != (1024, 1024):
        raise ValueError(f"unexpected metal_main dimensions: {(width, height)}")
    canvas = Canvas(width, height)

    # metal_main is used as a semantic atlas, not as pixel artwork. Each 64x64
    # tile is reduced to a new 4px modular silhouette. This keeps tile IDs and
    # edge connectivity useful to metal_main.rules while producing different
    # contours, hard foundry plates and fresh interior details.
    block = 4
    mask = bytearray(width * height)
    source_nonempty_tiles: set[int] = set()
    for tile in range(256):
        tile_x, tile_y = (tile % 16) * 64, (tile // 16) * 64
        if any(
            source[(y * width + x) * 4 + 3] > 24
            for y in range(tile_y, tile_y + 64)
            for x in range(tile_x, tile_x + 64)
        ):
            source_nonempty_tiles.add(tile)

    for by in range(0, height, block):
        for bx in range(0, width, block):
            samples: list[tuple[int, int, int, int]] = []
            for y in range(by, by + block):
                for x in range(bx, bx + block):
                    offset = (y * width + x) * 4
                    if source[offset + 3] > 24:
                        samples.append(tuple(source[offset : offset + 4]))  # type: ignore[arg-type]
            strong = sum(alpha > 144 for _r, _g, _b, alpha in samples)
            edge_contract = bx % 64 in (0, 60) or by % 64 in (0, 60)
            if not samples or (not edge_contract and strong == 0 and len(samples) < 5):
                continue
            for y in range(by, by + block):
                for x in range(bx, bx + block):
                    mask[y * width + x] = 1

    # Tiny semantic tiles (for example thin chain links) must not disappear.
    for tile in source_nonempty_tiles:
        tile_x, tile_y = (tile % 16) * 64, (tile // 16) * 64
        if any(mask[y * width + x] for y in range(tile_y, tile_y + 64) for x in range(tile_x, tile_x + 64)):
            continue
        strongest = max(
            (
                source[(y * width + x) * 4 + 3],
                x // block * block,
                y // block * block,
            )
            for y in range(tile_y, tile_y + 64)
            for x in range(tile_x, tile_x + 64)
        )
        _alpha, bx, by = strongest
        for y in range(by, by + block):
            for x in range(bx, bx + block):
                mask[y * width + x] = 1

    for y in range(height):
        for x in range(width):
            if not mask[y * width + x]:
                continue
            tile = (y // 64) * 16 + x // 64
            local_x, local_y = x % 64, y % 64
            variation = (tile * 13) % 11
            if tile in (145, 146):
                color = (238, 160 + variation, 31)
            elif tile in (147, 148, 149, 150):
                color = (178 + variation * 3, 48, 29)
            elif tile in (161, 162):
                color = (30, 135 + variation * 4, 78)
            else:
                grain = ((local_x // 4 * 3 + local_y // 4 * 5 + tile) % 9) - 4
                color = (50 + variation + grain, 59 + variation + grain, 68 + variation + grain)
                # Every atlas slot is treated as a modular plate, independent
                # of the original texture's internal pixels.
                if local_x in (8, 9) or local_y in (8, 9):
                    color = (30, 36, 43)
                elif local_x in (54, 55) or local_y in (54, 55):
                    color = (92, 103, 111)
            offset = (y * width + x) * 4
            canvas.pixels[offset : offset + 4] = bytes((*color, 255))

    # New hard bevels are derived from the generated mask, not source pixels.
    for y in range(height):
        for x in range(width):
            if not mask[y * width + x]:
                continue
            left = x == 0 or not mask[y * width + x - 1]
            top = y == 0 or not mask[(y - 1) * width + x]
            right = x == width - 1 or not mask[y * width + x + 1]
            bottom = y == height - 1 or not mask[(y + 1) * width + x]
            offset = (y * width + x) * 4
            if left or top:
                canvas.pixels[offset : offset + 3] = bytes((147, 160, 168))
            elif right or bottom:
                canvas.pixels[offset : offset + 3] = bytes((13, 17, 22))

    # Rules-driven structural tiles receive independent foundry panel language.
    structural_tiles = {17, 18, 19, 20, 34, 36, 40, 51, 84, 100, 101, 102, 103, 105, 106, 107, 109, 125}

    def paint_masked(x0: int, y0: int, x1: int, y1: int, color: Color) -> None:
        for py in range(max(0, y0), min(height, y1)):
            for px in range(max(0, x0), min(width, x1)):
                if mask[py * width + px]:
                    offset = (py * width + px) * 4
                    canvas.pixels[offset : offset + 4] = bytes(color)

    def line_masked(
        x0: int, y0: int, x1: int, y1: int, color: Color, line_width: int = 2
    ) -> None:
        dx, dy = x1 - x0, y1 - y0
        steps = max(abs(dx), abs(dy), 1)
        radius = max(0, line_width // 2)
        for step in range(steps + 1):
            px = round(x0 + dx * step / steps)
            py = round(y0 + dy * step / steps)
            paint_masked(px - radius, py - radius, px + radius + 1, py + radius + 1, color)

    def frame_masked(
        x0: int, y0: int, x1: int, y1: int, outer: Color, inner: Color
    ) -> None:
        paint_masked(x0, y0, x1, y0 + 4, outer)
        paint_masked(x0, y1 - 4, x1, y1, outer)
        paint_masked(x0, y0, x0 + 4, y1, outer)
        paint_masked(x1 - 4, y0, x1, y1, outer)
        paint_masked(x0 + 5, y0 + 5, x1 - 5, y0 + 7, inner)
        paint_masked(x0 + 5, y0 + 5, x0 + 7, y1 - 5, inner)

    def rivet(px: int, py: int, hot: bool = False) -> None:
        paint_masked(px - 3, py - 3, px + 4, py + 4, (18, 22, 27, 255))
        paint_masked(
            px - 1,
            py - 1,
            px + 2,
            py + 2,
            (255, 112, 27, 255) if hot else (158, 171, 178, 255),
        )

    def vent_panel(
        x0: int, y0: int, x1: int, y1: int, slats: int, diagonal: bool = False
    ) -> None:
        paint_masked(x0, y0, x1, y1, (24, 29, 35, 255))
        frame_masked(x0, y0, x1, y1, (135, 148, 157, 255), (79, 91, 101, 255))
        gap = max(8, (y1 - y0 - 18) // max(1, slats))
        for line_y in range(y0 + 12, y1 - 7, gap):
            paint_masked(x0 + 9, line_y, x1 - 9, line_y + 4, (84, 96, 105, 255))
            paint_masked(x0 + 9, line_y + 4, x1 - 9, line_y + 6, (13, 17, 22, 255))
        if diagonal:
            line_masked(x0 + 13, y0 + 13, x1 - 13, y1 - 13, (17, 21, 26, 255), 12)
            line_masked(x0 + 13, y0 + 10, x1 - 13, y1 - 16, (180, 80, 32, 255), 4)
        for px, py in ((x0 + 8, y0 + 8), (x1 - 9, y0 + 8), (x0 + 8, y1 - 9), (x1 - 9, y1 - 9)):
            rivet(px, py)

    def hazard_bar(x0: int, y0: int, x1: int, y1: int) -> None:
        paint_masked(x0, y0, x1, y1, (18, 22, 27, 255))
        paint_masked(x0 + 3, y0 + 3, x1 - 3, y1 - 3, (247, 155, 27, 255))
        stripe = max(8, (y1 - y0) * 2)
        for start in range(x0 - (y1 - y0), x1, stripe):
            for offset in range(y1 - y0):
                paint_masked(start + offset, y0 + offset, start + offset + stripe // 2, y0 + offset + 1, (29, 33, 37, 255))

    for tile in structural_tiles:
        tx, ty = (tile % 16) * 64, (tile // 16) * 64
        # Recessed seams and orange ceramic fasteners distinguish the theme
        # without changing which atlas slot the automapper addresses.
        paint_masked(tx + 7, ty + 8, tx + 9, ty + 56, (31, 37, 44, 255))
        paint_masked(tx + 55, ty + 8, tx + 57, ty + 56, (103, 114, 122, 255))
        for px, py in ((tx + 11, ty + 11), (tx + 52, ty + 11), (tx + 11, ty + 52), (tx + 52, ty + 52)):
            paint_masked(px - 2, py - 2, px + 3, py + 3, (225, 91, 25, 255))
    for tile in (51, 84, 100, 101, 102, 103):
        tx, ty = (tile % 16) * 64, (tile // 16) * 64
        paint_masked(tx + 10, ty + 47, tx + 54, ty + 53, (255, 91, 24, 255))
        paint_masked(tx + 17, ty + 48, tx + 47, ty + 50, (255, 186, 47, 255))

    # Distinct atlas structures reconstructed from the original tile roles.
    # Large blast shutters: separate sizes, slat counts and diagonal braces.
    vent_panel(596, 82, 754, 242, 6, True)
    vent_panel(790, 146, 882, 242, 4, True)

    # Access doors and service covers make adjacent solid tiles readable.
    vent_panel(74, 201, 183, 298, 3)
    vent_panel(202, 201, 252, 255, 2)
    vent_panel(327, 199, 505, 379, 5)
    vent_panel(73, 329, 249, 379, 1)
    vent_panel(72, 393, 249, 477, 2)
    line_masked(221, 286, 286, 286, (27, 32, 38, 255), 7)
    line_masked(286, 286, 286, 256, (179, 74, 31, 255), 4)
    for px in (266, 307, 348, 389, 430, 471):
        rivet(px, 359, px in (307, 430))
    hazard_bar(278, 466, 426, 489)

    # Suspended rails, two platform variants and the hanging chain assembly.
    frame_masked(584, 355, 824, 376, (151, 163, 169, 255), (79, 91, 99, 255))
    frame_masked(578, 386, 834, 452, (151, 163, 169, 255), (79, 91, 99, 255))
    frame_masked(578, 454, 838, 515, (130, 142, 150, 255), (69, 80, 89, 255))
    hazard_bar(758, 398, 818, 420)
    hazard_bar(592, 468, 678, 491)
    hazard_bar(723, 468, 832, 491)
    for px in (594, 654, 714, 774, 821):
        rivet(px, 441, px in (654, 774))
    line_masked(863, 368, 863, 516, (16, 20, 25, 255), 10)
    for link_y in range(375, 510, 25):
        frame_masked(855, link_y, 872, link_y + 22, (153, 165, 171, 255), (68, 78, 86, 255))

    # Expanded-metal guard sections: orange rail caps and alternating braces.
    frame_masked(590, 576, 900, 708, (132, 145, 153, 255), (68, 79, 88, 255))
    frame_masked(574, 766, 900, 1018, (132, 145, 153, 255), (68, 79, 88, 255))
    for start_x in range(592, 928, 48):
        line_masked(start_x, 590, start_x + 92, 704, (116, 128, 136, 255), 4)
        line_masked(start_x + 92, 590, start_x, 704, (36, 43, 50, 255), 4)
        line_masked(start_x - 16, 782, start_x + 144, 1004, (116, 128, 136, 255), 4)
        line_masked(start_x + 144, 782, start_x - 16, 1004, (36, 43, 50, 255), 4)
    line_masked(600, 581, 892, 581, (228, 88, 26, 255), 4)
    line_masked(584, 772, 892, 772, (228, 88, 26, 255), 4)

    # Lower wall/roof variants receive directional edge trims and service bolts.
    line_masked(192, 773, 192, 925, (154, 166, 173, 255), 5)
    line_masked(320, 773, 320, 925, (228, 88, 26, 255), 4)
    line_masked(448, 706, 448, 961, (154, 166, 173, 255), 5)
    line_masked(385, 919, 449, 961, (228, 88, 26, 255), 4)
    frame_masked(64, 897, 257, 923, (151, 164, 171, 255), (70, 82, 91, 255))
    for px, py in ((203, 783), (331, 783), (459, 719), (401, 918), (482, 979)):
        rivet(px, py, True)

    # New safety glyphs occupy the same semantic sign slots but use different
    # foundry symbols and construction from metal_main.
    # Tile 145: thermal spark; tile 146: general warning.
    for angle in range(0, 360, 45):
        radians = math.radians(angle)
        line_masked(96, 608, round(96 + math.cos(radians) * 17), round(608 + math.sin(radians) * 17), (24, 28, 33, 255), 4)
    paint_masked(93, 605, 100, 612, (255, 231, 126, 255))
    paint_masked(157, 592, 164, 618, (24, 28, 33, 255))
    paint_masked(157, 623, 164, 630, (24, 28, 33, 255))

    # Two red signs retain distinct jobs: interlocked gear and toxic smelter.
    paint_masked(220, 590, 308, 638, (226, 220, 188, 255))
    frame_masked(218, 588, 310, 640, (113, 38, 28, 255), (255, 111, 27, 255))
    paint_masked(246, 600, 282, 630, (29, 33, 37, 255))
    for px, py in ((246, 599), (278, 599), (246, 628), (278, 628), (239, 611), (285, 611)):
        paint_masked(px, py, px + 8, py + 8, (29, 33, 37, 255))
    paint_masked(258, 610, 270, 626, (230, 92, 26, 255))
    paint_masked(350, 590, 438, 638, (226, 220, 188, 255))
    frame_masked(348, 588, 440, 640, (113, 38, 28, 255), (255, 111, 27, 255))
    paint_masked(370, 598, 418, 624, (28, 32, 36, 255))
    paint_masked(376, 619, 412, 632, (28, 32, 36, 255))
    paint_masked(378, 604, 387, 613, (255, 111, 27, 255))
    paint_masked(401, 604, 410, 613, (255, 111, 27, 255))
    for tooth_x in (380, 391, 402):
        paint_masked(tooth_x, 628, tooth_x + 6, 636, (226, 220, 188, 255))

    # Exit sign is a two-tile composition: worker silhouette then direction.
    paint_masked(75, 650, 181, 695, (28, 139, 78, 255))
    frame_masked(73, 648, 183, 697, (19, 65, 43, 255), (78, 190, 117, 255))
    paint_masked(95, 657, 104, 666, (222, 242, 220, 255))
    line_masked(99, 666, 94, 682, (222, 242, 220, 255), 6)
    line_masked(96, 670, 85, 678, (222, 242, 220, 255), 5)
    line_masked(96, 681, 87, 691, (222, 242, 220, 255), 5)
    line_masked(96, 681, 108, 691, (222, 242, 220, 255), 5)
    line_masked(126, 673, 166, 673, (222, 242, 220, 255), 7)
    line_masked(156, 662, 169, 673, (222, 242, 220, 255), 7)
    line_masked(156, 684, 169, 673, (222, 242, 220, 255), 7)
    canvas.save_png(path)


def build_far_silhouette(path: Path) -> None:
    canvas = Canvas(1024, 512)
    base = (38, 42, 48, 235)
    canvas.rect(0, 390, 1024, 512, base)
    buildings = [(0, 245, 160, 400), (190, 290, 330, 400), (365, 215, 555, 400), (600, 275, 770, 400), (815, 230, 1024, 400)]
    for x0, y0, x1, y1 in buildings:
        canvas.rect(x0, y0, x1, y1, base)
        canvas.polygon([(x0, y0), ((x0 + x1) // 2, y0 - 42), (x1, y0)], base)
        for x in range(x0 + 24, x1 - 10, 42):
            canvas.rect(x, y0 + 38, x + 18, y0 + 52, (255, 91, 27, 115))
    for x, top, width in ((86, 70, 42), (286, 130, 34), (484, 45, 50), (702, 105, 38), (921, 58, 46)):
        canvas.rect(x, top, x + width, 390, (31, 35, 41, 245))
        canvas.rect(x - 6, top, x + width + 6, top + 13, STEEL_DARK)
        canvas.polygon([(x - 3, top), (x + width + 3, top), (x + width - 4, top - 16), (x + 4, top - 16)], STEEL)
    canvas.save_png(path)


def build_glow_towers(path: Path) -> None:
    canvas = Canvas(512, 512)
    for cx, height, width in ((82, 420, 96), (250, 350, 88), (430, 408, 92)):
        x0, x1 = cx - width // 2, cx + width // 2
        y0 = 512 - height
        canvas.polygon([(x0, 512), (x0 + 15, y0 + 22), (cx, y0), (x1 - 15, y0 + 22), (x1, 512)], (39, 44, 50, 230))
        for y in range(y0 + 60, 480, 78):
            canvas.rect(x0 + 22, y, x1 - 22, y + 12, (255, 92, 25, 120))
            canvas.rect(x0 + 29, y + 3, x1 - 29, y + 7, (255, 191, 51, 120))
    canvas.save_png(path)


def build_hall_background(path: Path) -> None:
    canvas = Canvas(1024, 1024)
    canvas.gradient((22, 27, 35, 255), (59, 39, 39, 255))
    # Roof trusses and distant vertical supports.
    for x in range(-120, 1120, 220):
        canvas.line(x, 0, x + 220, 250, (15, 19, 24, 255), 18)
        canvas.line(x + 220, 0, x, 250, (15, 19, 24, 255), 18)
    for x in range(0, 1024, 128):
        canvas.rect(x, 210, x + 18, 1024, (30, 35, 42, 255))
        canvas.rect(x + 18, 210, x + 23, 1024, (88, 70, 68, 255))
    # Furnace bays provide the strong orange foundry identity.
    for x in (82, 382, 682):
        canvas.rect(x, 420, x + 240, 880, (25, 28, 32, 255))
        canvas.frame(x, 420, x + 240, 880, STEEL, 12)
        canvas.polygon([(x + 32, 820), (x + 58, 560), (x + 182, 560), (x + 208, 820)], (76, 46, 39, 255))
        canvas.rect(x + 48, 610, x + 192, 820, (173, 45, 20, 255))
        canvas.rect(x + 60, 650, x + 180, 820, ORANGE)
        canvas.rect(x + 78, 700, x + 162, 820, AMBER)
        canvas.rect(x + 96, 760, x + 144, 820, (255, 239, 145, 255))
        hazard_stripes(canvas, x + 18, 835, 204, 20)
    canvas.rect(0, 900, 1024, 1024, (30, 32, 36, 255))
    canvas.line(0, 903, 1024, 903, STEEL_LIGHT, 7)
    canvas.save_png(path)


def build_mid_pipes(path: Path) -> None:
    canvas = Canvas(1024, 512)
    pipes = [(25, 88, 930, 46), (180, 210, 1010, 34), (-80, 340, 760, 54)]
    for x0, y, x1, width in pipes:
        canvas.line(x0, y, x1, y, INK, width + 10)
        canvas.line(x0, y, x1, y, STEEL_DARK, width)
        canvas.line(x0, y - width // 5, x1, y - width // 5, STEEL, 5)
        for x in range(max(0, x0 + 70), min(1024, x1), 150):
            canvas.rect(x - 8, y - width // 2 - 9, x + 8, y + width // 2 + 9, INK)
            canvas.rect(x - 4, y - width // 2 - 7, x + 4, y + width // 2 + 7, EDGE)
    for x, y in ((130, 88), (855, 210), (650, 340)):
        canvas.ellipse((x - 27, y - 27, x + 27, y + 27), INK)
        canvas.ellipse((x - 20, y - 20, x + 20, y + 20), STEEL)
        canvas.ellipse((x - 8, y - 8, x + 8, y + 8), CYAN)
    canvas.save_png(path)


def build_smoke_and_crane(path: Path) -> None:
    canvas = Canvas(1024, 256)
    # Overhead crane silhouette.
    canvas.rect(0, 12, 1024, 38, (32, 37, 43, 245))
    hazard_stripes(canvas, 0, 38, 1024, 10)
    for x in range(40, 1024, 160):
        canvas.polygon([(x, 48), (x + 18, 48), (x + 94, 150), (x + 76, 150)], (31, 35, 41, 230))
    canvas.rect(420, 46, 600, 72, STEEL_DARK)
    canvas.rect(500, 70, 514, 172, INK)
    canvas.ellipse((481, 162, 533, 214), INK)
    canvas.ellipse((493, 174, 521, 202), ORANGE)
    # Sparse hot smoke, intentionally translucent.
    for cx, cy, rx, ry in ((120, 180, 100, 48), (270, 210, 130, 42), (760, 185, 125, 50), (920, 220, 100, 38)):
        canvas.ellipse((cx - rx, cy - ry, cx + rx, cy + ry), (75, 57, 55, 95))
    canvas.save_png(path)


def build_doodads(path: Path) -> None:
    canvas = Canvas(1024, 1024)
    # Atlas top row: wall machinery, ducts and a suspended smelter.
    panel(canvas, 0, 0, 270, 270, False)
    for y in range(62, 225, 34):
        canvas.rect(38, y, 220, y + 12, (42, 49, 56, 255))
        canvas.rect(44, y + 2, 214, y + 5, STEEL_LIGHT)
    panel(canvas, 310, 0, 250, 270, True)
    canvas.ellipse((378, 54, 494, 170), INK)
    canvas.ellipse((392, 68, 480, 156), ORANGE)
    canvas.ellipse((414, 90, 458, 134), AMBER)
    for y in (78, 184, 216):
        canvas.rect(560, y, 780, y + 28, INK)
        canvas.rect(570, y + 5, 770, y + 21, STEEL)
    canvas.rect(810, 0, 842, 270, INK)
    canvas.rect(842, 0, 1024, 270, STEEL_DARK)
    for y in range(25, 260, 48):
        canvas.line(850, y, 1010, y, STEEL, 8)
    # Lower atlas area: molten trough, crates, hanging chains and warning lamps.
    canvas.polygon([(0, 350), (330, 350), (290, 530), (38, 530)], INK)
    canvas.polygon([(25, 375), (305, 375), (272, 492), (54, 492)], (114, 47, 31, 255))
    canvas.polygon([(38, 400), (292, 400), (260, 468), (65, 468)], ORANGE)
    canvas.polygon([(62, 416), (270, 416), (246, 447), (78, 447)], AMBER)
    for x in range(360, 690, 104):
        panel(canvas, x, 352, 88, 78, False)
        hazard_stripes(canvas, x + 8, 397, 72, 12)
    for x in (760, 850, 940):
        canvas.line(x, 320, x, 510, INK, 12)
        for y in range(332, 500, 28):
            canvas.ellipse((x - 10, y, x + 10, y + 22), EDGE)
            canvas.ellipse((x - 5, y + 4, x + 5, y + 18), TRANSPARENT)
    for x, color in ((110, HEAT), (205, AMBER), (300, CYAN)):
        canvas.rect(x - 5, 570, x + 5, 670, INK)
        canvas.ellipse((x - 25, 650, x + 25, 700), INK)
        canvas.ellipse((x - 17, 658, x + 17, 692), color)

    # Runtime map generation uses these exact atlas indices for chains and
    # two-tile-wide platform machinery. Keep them isolated and tile-safe.
    def clear_tile(index: int) -> tuple[int, int]:
        x, y = (index % 16) * 64, (index // 16) * 64
        canvas.rect(x, y, x + 64, y + 64, TRANSPARENT)
        return x, y

    x, y = clear_tile(1)
    canvas.line(x + 32, y - 4, x + 32, y + 68, INK, 10)
    for link_y in range(y + 2, y + 64, 18):
        canvas.ellipse((x + 23, link_y, x + 41, link_y + 16), EDGE)
        canvas.ellipse((x + 28, link_y + 4, x + 36, link_y + 12), TRANSPARENT)

    for index in (153, 154, 155):
        x, y = clear_tile(index)
        canvas.rect(x, y + 38, x + 64, y + 64, INK)
        canvas.rect(x, y + 42, x + 64, y + 59, STEEL)
        canvas.line(x, y + 43, x + 64, y + 43, EDGE, 3)
        for post_x in (x + 8, x + 56):
            canvas.rect(post_x - 3, y + 8, post_x + 3, y + 44, STEEL_LIGHT)
        canvas.line(x + 8, y + 12, x + 56, y + 36, STEEL_DARK, 5)
        canvas.line(x + 8, y + 36, x + 56, y + 12, STEEL_DARK, 5)
    for index in (169, 170, 171):
        x, y = clear_tile(index)
        panel(canvas, x, y, 64, 64, True)
        hazard_stripes(canvas, x + 3, y + 45, 58, 12)
    canvas.save_png(path)


def read_datafile(path: Path) -> tuple[bytes, tuple[int, ...], bytes, list[bytes]]:
    data = path.read_bytes()
    if data[:4] not in (b"DATA", b"ATAD"):
        raise ValueError(f"{path} is not a Teeworlds datafile")
    header = struct.unpack_from("<8i", data, 4)
    version, _size, _swaplen, type_count, item_count, raw_count, item_size, data_size = header
    if version != 4:
        raise ValueError(f"unsupported datafile version {version}")
    offset = 36
    type_bytes = data[offset : offset + type_count * 12]
    offset += type_count * 12
    item_offsets_bytes = data[offset : offset + item_count * 4]
    offset += item_count * 4
    raw_offsets = list(struct.unpack_from(f"<{raw_count}i", data, offset))
    offset += raw_count * 4
    raw_sizes = list(struct.unpack_from(f"<{raw_count}i", data, offset))
    offset += raw_count * 4
    item_bytes = data[offset : offset + item_size]
    compressed_start = offset + item_size
    raw_data: list[bytes] = []
    for index, raw_offset in enumerate(raw_offsets):
        raw_end = raw_offsets[index + 1] if index + 1 < raw_count else data_size
        unpacked = zlib.decompress(data[compressed_start + raw_offset : compressed_start + raw_end])
        if len(unpacked) != raw_sizes[index]:
            raise ValueError(f"raw block {index} has invalid size")
        raw_data.append(unpacked)
    metadata = type_bytes + item_offsets_bytes
    return data[:4], header, metadata + item_bytes, raw_data


def write_foundry_map(template: Path, output: Path) -> list[str]:
    magic, header, fixed_bytes, raw_data = read_datafile(template)
    version, _size, _swaplen, type_count, item_count, raw_count, item_size, _data_size = header
    source_names = [b"bg_stone1\0", b"gray-bg-2\0", b"metal_main\0", b"path-bg1\0", b"path-bg2\0", b"path-bg4\0", b"warehouse_background\0"]
    target_names = [b"foundry_bg_far\0", b"foundry_bg_glow\0", b"foundry_main\0", b"foundry_parallax_near\0", b"foundry_parallax_mid\0", b"foundry_parallax_smoke\0", b"foundry_doodads\0"]
    if raw_data[: len(source_names)] != source_names:
        actual = [block.rstrip(b"\0").decode("ascii", "replace") for block in raw_data[:7]]
        raise ValueError(f"unexpected template image names: {actual}")
    raw_data[: len(target_names)] = target_names

    # The stock template has seven image declarations but only four referenced
    # layers. Add two parallax layers and point the generated doodad tile layer
    # at image 6. All game/collision payloads remain untouched.
    metadata_size = type_count * 12 + item_count * 4
    type_bytes = fixed_bytes[: type_count * 12]
    item_offsets_bytes = fixed_bytes[type_count * 12 : metadata_size]
    old_item_bytes = fixed_bytes[metadata_size:]
    old_item_offsets = struct.unpack(f"<{item_count}i", item_offsets_bytes)
    items: list[tuple[int, int, bytes]] = []
    for index, item_offset in enumerate(old_item_offsets):
        type_and_id, payload_size = struct.unpack_from("<2i", old_item_bytes, item_offset)
        item_type = (type_and_id >> 16) & 0xFFFF
        item_id = type_and_id & 0xFFFF
        payload_start = item_offset + 8
        payload = old_item_bytes[payload_start : payload_start + payload_size]
        items.append((item_type, item_id, payload))

    layers = {item_id: payload for item_type, item_id, payload in items if item_type == 5}
    if sorted(layers) != list(range(8)):
        raise ValueError("unexpected generate_large3 layer layout")

    def set_int(payload: bytes, index: int, value: int) -> bytes:
        values = list(struct.unpack(f"<{len(payload) // 4}i", payload))
        values[index] = value
        return struct.pack(f"<{len(values)}i", *values)

    new_items: list[tuple[int, int, bytes]] = []
    emitted_layers = False
    for item_type, item_id, payload in items:
        if item_type == 4:
            values = list(struct.unpack(f"<{len(payload) // 4}i", payload))
            if item_id == 1:
                values[6] = 3  # existing near hall + two new overlay layers
            elif item_id >= 2:
                values[5] += 2  # layer start shifts after the insertion
            payload = struct.pack(f"<{len(values)}i", *values)
        if item_type != 5:
            new_items.append((item_type, item_id, payload))
            continue
        if emitted_layers:
            continue
        emitted_layers = True
        ordered_layers = [
            layers[0],
            layers[1],
            set_int(layers[1], 6, 4),  # transparent pipe overlay
            set_int(layers[1], 6, 5),  # transparent crane/smoke overlay
            layers[2],
            layers[3],
            layers[4],
            set_int(layers[5], 13, 6),  # doodads use the dedicated doodad atlas
            layers[6],
            set_int(layers[7], 13, 2),  # foreground geometry uses foundry_main
        ]
        for new_id, layer_payload in enumerate(ordered_layers):
            new_items.append((5, new_id, layer_payload))

    item_offsets: list[int] = []
    rebuilt_items = bytearray()
    for item_type, item_id, payload in new_items:
        item_offsets.append(len(rebuilt_items))
        rebuilt_items.extend(struct.pack("<2i", (item_type << 16) | item_id, len(payload)))
        rebuilt_items.extend(payload)
    item_count = len(new_items)
    item_size = len(rebuilt_items)
    metadata_size = type_count * 12 + item_count * 4

    type_rows: list[tuple[int, int, int]] = []
    cursor = 0
    for item_type in sorted({item_type for item_type, _item_id, _payload in new_items}):
        count = sum(1 for current_type, _item_id, _payload in new_items if current_type == item_type)
        type_rows.append((item_type, cursor, count))
        cursor += count
    if len(type_rows) != type_count:
        raise ValueError("map item type count changed unexpectedly")
    type_bytes = b"".join(struct.pack("<3i", *row) for row in type_rows)

    compressed = [zlib.compress(block, 9) for block in raw_data]
    raw_offsets: list[int] = []
    cursor = 0
    for block in compressed:
        raw_offsets.append(cursor)
        cursor += len(block)
    raw_sizes = [len(block) for block in raw_data]
    type_and_item_offsets = type_bytes + struct.pack(f"<{item_count}i", *item_offsets)
    item_bytes = bytes(rebuilt_items)
    swap_size = 36 + metadata_size + raw_count * 8 + item_size
    file_size = swap_size + cursor
    new_header = struct.pack(
        "<4s8i",
        magic,
        version,
        file_size - 16,
        swap_size - 16,
        type_count,
        item_count,
        raw_count,
        item_size,
        cursor,
    )
    rebuilt = bytearray(new_header)
    rebuilt.extend(type_and_item_offsets)
    rebuilt.extend(struct.pack(f"<{raw_count}i", *raw_offsets))
    rebuilt.extend(struct.pack(f"<{raw_count}i", *raw_sizes))
    rebuilt.extend(item_bytes)
    for block in compressed:
        rebuilt.extend(block)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(rebuilt)
    return [name.rstrip(b"\0").decode("ascii") for name in target_names]


def png_info(path: Path) -> tuple[int, int, int]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path} is not a PNG")
    width, height, _depth, color_type = struct.unpack_from(">IIBB", data, 16)
    return width, height, color_type


def validate_tileset_correspondence(source_path: Path, themed_path: Path) -> None:
    source_width, source_height, source = load_rgba_png(source_path)
    themed_width, themed_height, themed = load_rgba_png(themed_path)
    if (source_width, source_height) != (themed_width, themed_height):
        raise ValueError("foundry_main dimensions do not match metal_main")
    source_alpha = source[3::4]
    themed_alpha = themed[3::4]
    source_tiles: set[int] = set()
    themed_tiles: set[int] = set()

    def tile_has(alpha: bytearray, tile: int) -> bool:
        tx, ty = (tile % 16) * 64, (tile // 16) * 64
        return any(
            alpha[y * source_width + x] > 24
            for y in range(ty, ty + 64)
            for x in range(tx, tx + 64)
        )

    for tile in range(256):
        if tile_has(source_alpha, tile):
            source_tiles.add(tile)
        if tile_has(themed_alpha, tile):
            themed_tiles.add(tile)
    if source_tiles != themed_tiles:
        raise ValueError(
            "foundry_main tile slots differ from metal_main: "
            f"missing={sorted(source_tiles - themed_tiles)}, extra={sorted(themed_tiles - source_tiles)}"
        )

    # Compare 4px edge bins, the actual seam contract needed when rules rotate
    # or join tiles. Interiors are intentionally allowed—and required—to differ.
    def edge_signature(alpha: bytearray, tile: int) -> tuple[bool, ...]:
        tx, ty = (tile % 16) * 64, (tile // 16) * 64
        signature: list[bool] = []
        for side in range(4):
            for section in range(16):
                if side == 0:  # top
                    points = ((tx + section * 4 + dx, ty + dy) for dy in range(4) for dx in range(4))
                elif side == 1:  # right
                    points = ((tx + 60 + dx, ty + section * 4 + dy) for dy in range(4) for dx in range(4))
                elif side == 2:  # bottom
                    points = ((tx + section * 4 + dx, ty + 60 + dy) for dy in range(4) for dx in range(4))
                else:  # left
                    points = ((tx + dx, ty + section * 4 + dy) for dy in range(4) for dx in range(4))
                signature.append(any(alpha[y * source_width + x] > 24 for x, y in points))
        return tuple(signature)

    bad_edges = [
        tile
        for tile in source_tiles
        if edge_signature(source_alpha, tile) != edge_signature(themed_alpha, tile)
    ]
    if bad_edges:
        raise ValueError(f"foundry_main edge contracts differ for tiles {bad_edges}")

    union = sum((source_alpha[i] > 24) or (themed_alpha[i] > 24) for i in range(len(source_alpha)))
    silhouette_changes = sum(
        (source_alpha[i] > 24) != (themed_alpha[i] > 24) for i in range(len(source_alpha))
    )
    color_changes = sum(
        source[i : i + 3] != themed[i : i + 3]
        for i in range(0, len(source), 4)
        if source[i + 3] > 24 or themed[i + 3] > 24
    )
    if union == 0 or color_changes * 100 < union * 20:
        raise ValueError("foundry_main palette is still too close to metal_main")
    if silhouette_changes * 100 > union * 45:
        raise ValueError("foundry_main changed too much to preserve tile semantics")


def validate_native_asset_roles(mapres: Path) -> None:
    """Guard the visual-layer contracts used by the generated map."""

    def alpha_values(filename: str) -> tuple[int, int, bytearray]:
        width, height, pixels = load_rgba_png(mapres / filename)
        return width, height, pixels[3::4]

    # Background layers must cover their complete quad. Overlay layers need
    # meaningful content but must retain enough transparency for parallax.
    for filename in ("foundry_bg_far.png", "foundry_bg_glow.png", "foundry_parallax_near.png"):
        width, height, alpha = alpha_values(filename)
        if any(value == 0 for value in alpha):
            raise ValueError(f"opaque foundry background contains holes: {filename}")
        if len(alpha) != width * height:
            raise ValueError(f"invalid alpha plane: {filename}")
    for filename, minimum, maximum in (
        ("foundry_parallax_mid.png", 0.12, 0.55),
        ("foundry_parallax_smoke.png", 0.18, 0.65),
        ("foundry_doodads.png", 0.08, 0.40),
    ):
        _width, _height, alpha = alpha_values(filename)
        coverage = sum(value > 0 for value in alpha) / len(alpha)
        if not minimum <= coverage <= maximum:
            raise ValueError(f"unexpected alpha coverage for {filename}: {coverage:.1%}")

    _source_width, _source_height, source = load_rgba_png(mapres / "metal_main.png")
    _theme_width, _theme_height, themed = load_rgba_png(mapres / "foundry_main.png")
    source_alpha = source[3::4]
    themed_alpha = themed[3::4]
    union = sum((source_alpha[index] > 24) or (themed_alpha[index] > 24) for index in range(len(source_alpha)))
    silhouette_changes = sum(
        (source_alpha[index] > 24) != (themed_alpha[index] > 24)
        for index in range(len(source_alpha))
    )
    silhouette_ratio = silhouette_changes / max(1, union)
    if not 0.02 <= silhouette_ratio <= 0.35:
        raise ValueError(f"foundry_main needs a distinct but compatible interior silhouette: {silhouette_ratio:.1%}")
    color_delta = 0
    samples = 0
    for offset in range(0, len(source), 4):
        if source[offset + 3] <= 24 and themed[offset + 3] <= 24:
            continue
        color_delta += sum(abs(source[offset + channel] - themed[offset + channel]) for channel in range(3))
        samples += 3
    mean_delta = color_delta / max(1, samples)
    if mean_delta < 32.0:
        raise ValueError(f"foundry_main palette/interior remains too similar to metal_main: {mean_delta:.1f}")

    # Collision geometry must stay visibly in front of the opaque far wall,
    # even after both are darkened by the map renderer. Guard both luminance
    # separation and the cool-foreground/warm-background color language.
    _far_width, _far_height, far = load_rgba_png(mapres / "foundry_bg_far.png")

    def opaque_mean_rgb(pixels: bytearray) -> tuple[float, float, float]:
        totals = [0, 0, 0]
        count = 0
        for offset in range(0, len(pixels), 4):
            if pixels[offset + 3] <= 24:
                continue
            for channel in range(3):
                totals[channel] += pixels[offset + channel]
            count += 1
        return tuple(total / max(1, count) for total in totals)

    main_rgb = opaque_mean_rgb(themed)
    far_rgb = opaque_mean_rgb(far)
    main_luma = 0.2126 * main_rgb[0] + 0.7152 * main_rgb[1] + 0.0722 * main_rgb[2]
    far_luma = 0.2126 * far_rgb[0] + 0.7152 * far_rgb[1] + 0.0722 * far_rgb[2]
    if main_luma - far_luma < 45.0:
        raise ValueError(f"foundry collision wall is too close to the background luminance: {main_luma:.1f} vs {far_luma:.1f}")
    if (main_rgb[2] - main_rgb[0]) - (far_rgb[2] - far_rgb[0]) < 8.0:
        raise ValueError("foundry collision wall lacks a distinct cool foreground hue")

    width, _height, doodads = load_rgba_png(mapres / "foundry_doodads.png")
    doodad_alpha = doodads[3::4]
    for tile in (1, 153, 154, 155, 169, 170, 171):
        tx, ty = (tile % 16) * 64, (tile // 16) * 64
        coverage = sum(
            doodad_alpha[y * width + x] > 0
            for y in range(ty, ty + 64)
            for x in range(tx, tx + 64)
        )
        if coverage < 96:
            raise ValueError(f"required foundry doodad tile is empty or too thin: {tile}")


def referenced_map_images(path: Path) -> set[int]:
    _magic, header, fixed_bytes, _raw_data = read_datafile(path)
    _version, _size, _swaplen, type_count, item_count, _raw_count, item_size, _data_size = header
    metadata_size = type_count * 12 + item_count * 4
    offsets_start = type_count * 12
    item_offsets = struct.unpack_from(f"<{item_count}i", fixed_bytes, offsets_start)
    item_bytes = fixed_bytes[metadata_size : metadata_size + item_size]
    images: set[int] = set()
    for item_offset in item_offsets:
        type_and_id, payload_size = struct.unpack_from("<2i", item_bytes, item_offset)
        if ((type_and_id >> 16) & 0xFFFF) != 5:
            continue
        payload = item_bytes[item_offset + 8 : item_offset + 8 + payload_size]
        values = struct.unpack(f"<{len(payload) // 4}i", payload)
        layer_type = values[1]
        image = values[6] if layer_type == 3 else values[13] if layer_type == 2 else -1
        if image >= 0:
            images.add(image)
    return images


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


# Native Ninslash-style redraw. These builders deliberately preserve the
# smooth outlines and 64px tile contracts of the existing map resources.
def build_main_tileset_native(path: Path) -> None:
    original = Image.open(path.with_name("metal_main.png")).convert("RGBA")
    original_alpha = original.getchannel("A")

    # Only the four-pixel tile borders are compatibility contracts. Close the
    # old diamond-mesh holes inside the two guard regions, then restore every
    # tile border from metal_main so rotations and joins remain exact.
    alpha = original_alpha.copy()
    closed_alpha = original_alpha.filter(ImageFilter.MaxFilter(31))
    for box in ((584, 572, 904, 712), (568, 758, 906, 1024)):
        alpha.paste(closed_alpha.crop(box), box)
    alpha_draw = ImageDraw.Draw(alpha)
    for tile in range(256):
        tx, ty = (tile % 16) * 64, (tile // 16) * 64
        if original_alpha.crop((tx, ty, tx + 64, ty + 64)).getbbox() is None:
            alpha_draw.rectangle((tx, ty, tx + 63, ty + 63), fill=0)
    for ty in range(0, 1024, 64):
        for tx in range(0, 1024, 64):
            alpha.paste(original_alpha.crop((tx, ty, tx + 64, ty + 4)), (tx, ty))
            alpha.paste(original_alpha.crop((tx, ty + 60, tx + 64, ty + 64)), (tx, ty + 60))
            alpha.paste(original_alpha.crop((tx, ty, tx + 4, ty + 64)), (tx, ty))
            alpha.paste(original_alpha.crop((tx + 60, ty, tx + 64, ty + 64)), (tx + 60, ty))

    # Repaint all occupied pixels from the semantic mask. No RGB shading or
    # interior line from metal_main survives this point.
    source = Image.new("RGBA", original.size, (0, 0, 0, 0))
    source_pixels = source.load()
    alpha_pixels = alpha.load()
    for y in range(1024):
        for x in range(1024):
            opacity = alpha_pixels[x, y]
            if opacity == 0:
                continue
            # Foreground collision must remain legible after Ninslash's dark
            # map-layer tint. Use a cool, substantially lighter steel family;
            # the far wall deliberately stays charcoal and warm-neutral.
            broad_variation = ((x // 96) * 5 + (y // 96) * 3) % 13 - 6
            color = (101 + broad_variation, 125 + broad_variation, 143 + broad_variation)
            top = y == 0 or alpha_pixels[x, y - 1] < 32
            left = x == 0 or alpha_pixels[x - 1, y] < 32
            bottom = y == 1023 or alpha_pixels[x, y + 1] < 32
            right = x == 1023 or alpha_pixels[x + 1, y] < 32
            if top or left:
                color = (207, 225, 235)
            elif bottom or right:
                color = (31, 42, 52)
            source_pixels[x, y] = (*color, opacity)

    scale = 2
    details = Image.new("RGBA", (2048, 2048), (0, 0, 0, 0))
    draw = ImageDraw.Draw(details)
    ink = (27, 35, 43, 255)
    shadow = (66, 81, 93, 255)
    steel = (126, 148, 163, 255)
    light = (215, 229, 237, 255)
    refractory = (132, 79, 59, 255)
    orange = (238, 77, 22, 255)
    amber = (255, 177, 52, 255)

    def box(values):
        return tuple(round(value * scale) for value in values)

    def rect(values, fill, outline=None, width=1, radius=3):
        draw.rounded_rectangle(box(values), radius=radius * scale, fill=fill, outline=outline, width=width * scale)

    def ellipse(values, fill=None, outline=None, width=1):
        draw.ellipse(box(values), fill=fill, outline=outline, width=width * scale)

    def line(points, fill, width=1):
        draw.line([(x * scale, y * scale) for x, y in points], fill=fill, width=width * scale, joint="curve")

    def polygon(points, fill):
        draw.polygon([(x * scale, y * scale) for x, y in points], fill=fill)

    def bolt(x, y, hot=False):
        ellipse((x - 4, y - 4, x + 4, y + 4), ink)
        ellipse((x - 2, y - 2, x + 2, y + 2), amber if hot else light)

    # Automapper surfaces: cast refractory blocks and ceramic heat keys replace
    # the original white bevel language while preserving all join edges.
    for index in (17, 18, 19, 20, 34, 36, 40, 51, 84, 100, 101, 102, 103, 105, 106, 107, 109, 125):
        tx, ty = (index % 16) * 64, (index // 16) * 64
        line(((tx + 12, ty + 12), (tx + 52, ty + 12)), light, 2)
        line(((tx + 12, ty + 17), (tx + 12, ty + 48)), shadow, 3)
        rect((tx + 19, ty + 48, tx + 45, ty + 56), refractory, ink, 2, 4)
        line(((tx + 23, ty + 51), (tx + 41, ty + 51)), orange, 3)
        bolt(tx + 13, ty + 18, index % 3 == 0)
        bolt(tx + 51, ty + 18)

    # Pressure-furnace hatches replace metal_main's signature diagonal shutters.
    for cx, cy, radius in ((674, 162, 65), (834, 194, 38)):
        ellipse((cx - radius, cy - radius, cx + radius, cy + radius), shadow, ink, 7)
        ellipse((cx - radius + 11, cy - radius + 11, cx + radius - 11, cy + radius - 11), (31, 36, 41, 255), light, 4)
        for angle in range(0, 360, 45):
            radians = math.radians(angle)
            line(((cx, cy), (cx + math.cos(radians) * (radius - 17), cy + math.sin(radians) * (radius - 17))), steel, 7)
            bolt(cx + math.cos(radians) * (radius - 5), cy + math.sin(radians) * (radius - 5), angle in (90, 270))
        ellipse((cx - radius * 0.29, cy - radius * 0.29, cx + radius * 0.29, cy + radius * 0.29), refractory, ink, 5)
        ellipse((cx - radius * 0.15, cy - radius * 0.15, cx + radius * 0.15, cy + radius * 0.15), orange, amber, 3)

    # Heat exchangers, gauges and a radial pump replace the stock access doors.
    rect((74, 201, 183, 298), shadow, ink, 5, 10)
    for y in (220, 240, 260, 280):
        line(((92, y), (165, y)), steel, 8)
        line(((95, y - 2), (162, y - 2)), light, 2)
    for y in (220, 240, 260):
        line(((165, y), (174, y + 10), (165, y + 20)), refractory, 5)
    rect((202, 201, 252, 255), shadow, ink, 4, 8)
    ellipse((211, 207, 243, 239), (56, 63, 69, 255), ink, 4)
    line(((227, 223), (237, 213)), amber, 3)
    rect((211, 242, 243, 250), refractory, ink, 2, 3)

    rect((327, 199, 505, 379), shadow, ink, 6, 13)
    ellipse((352, 224, 480, 352), (31, 36, 41, 255), light, 5)
    ellipse((373, 245, 459, 331), refractory, ink, 5)
    for angle in range(0, 360, 60):
        radians = math.radians(angle)
        line(((416, 288), (416 + math.cos(radians) * 54, 288 + math.sin(radians) * 54)), steel, 10)
    ellipse((398, 270, 434, 306), orange, ink, 5)
    ellipse((407, 279, 425, 297), amber, None, 1)
    for x in (348, 484):
        for y in (220, 358):
            bolt(x, y, x == 484 and y == 358)

    # Conveyor rollers and refractory service modules occupy the old flat panels.
    rect((73, 329, 249, 379), shadow, ink, 5, 9)
    line(((86, 350), (236, 350)), steel, 15)
    for x in range(97, 232, 27):
        ellipse((x - 9, 341, x + 9, 359), (54, 61, 67, 255), ink, 3)
        ellipse((x - 3, 347, x + 3, 353), amber if x == 178 else light)
    rect((72, 393, 249, 477), (47, 51, 56, 255), ink, 5, 10)
    for row in range(3):
        offset = 12 if row % 2 else 0
        for x in range(82 - offset, 250, 36):
            rect((x, 402 + row * 22, x + 29, 419 + row * 22), refractory, shadow, 2, 4)
    line(((88, 466), (234, 466)), orange, 7)
    line(((98, 463), (224, 463)), amber, 2)
    for x in (278, 342, 406, 470):
        rect((x - 15, 398, x + 33, 486), shadow, ink, 4, 8)
        line(((x - 7, 412), (x + 25, 412)), light, 2)
        rect((x - 4, 438, x + 18, 477), refractory, ink, 3, 5)
        line(((x, 468), (x + 14, 468)), orange, 5)

    # Suspended belts now read as casting conveyors rather than stock platforms.
    rect((582, 385, 832, 452), shadow, ink, 5, 8)
    line(((594, 407), (818, 407)), steel, 20)
    for x in range(608, 812, 29):
        ellipse((x - 10, 397, x + 10, 417), (54, 61, 67, 255), ink, 3)
        ellipse((x - 3, 404, x + 3, 410), light)
    line(((594, 437), (818, 437)), orange, 7)
    rect((578, 454, 838, 515), (52, 43, 40, 255), ink, 5, 8)
    for start in range(590, 828, 52):
        polygon(((start, 468), (start + 39, 468), (start + 28, 501), (start - 10, 501)), refractory)
        line(((start + 3, 484), (start + 27, 484)), amber, 4)
    line(((863, 369), (863, 516)), ink, 10)
    for y in range(376, 509, 24):
        ellipse((854, y, 872, y + 18), None, light, 5)

    # Rebuild all safety glyphs so no colored pixel from metal_main is reused.
    rect((64, 576, 192, 640), (247, 178, 32, 255), ink, 4, 7)
    for x in (96, 160):
        polygon(((x, 588), (x - 23, 630), (x + 23, 630)), (255, 207, 52, 255))
    for angle in range(0, 360, 45):
        radians = math.radians(angle)
        line(((96, 612), (96 + math.cos(radians) * 14, 612 + math.sin(radians) * 14)), ink, 4)
    ellipse((92, 608, 100, 616), ink)
    line(((160, 598), (160, 617)), ink, 6)
    ellipse((156, 622, 164, 630), ink)

    rect((208, 576, 320, 640), (193, 54, 34, 255), ink, 5, 8)
    rect((220, 586, 308, 632), (220, 213, 180, 255), ink, 3, 5)
    ellipse((246, 592, 284, 630), shadow, ink, 4)
    for angle in range(0, 360, 45):
        radians = math.radians(angle)
        line(((265, 611), (265 + math.cos(radians) * 17, 611 + math.sin(radians) * 17)), ink, 5)
    ellipse((258, 604, 272, 618), orange, ink, 2)

    rect((336, 576, 448, 640), (193, 54, 34, 255), ink, 5, 8)
    rect((348, 586, 436, 632), (220, 213, 180, 255), ink, 3, 5)
    rect((369, 594, 415, 620), shadow, ink, 3, 7)
    line(((378, 610), (406, 610)), orange, 6)
    for x in (377, 407):
        ellipse((x - 4, 625, x + 4, 633), ink)

    rect((64, 640, 192, 704), (31, 139, 77, 255), ink, 5, 8)
    rect((75, 650, 181, 695), (37, 164, 88, 255), (99, 210, 134, 255), 3, 5)
    ellipse((91, 656, 103, 668), (226, 244, 225, 255))
    line(((97, 668), (92, 684), (84, 691)), (226, 244, 225, 255), 6)
    line(((96, 673), (108, 682)), (226, 244, 225, 255), 5)
    line(((122, 674), (167, 674)), (226, 244, 225, 255), 7)
    line(((156, 663), (169, 674), (156, 685)), (226, 244, 225, 255), 7)

    # The old diamond mesh is filled and replaced by vertical ceramic heat
    # shields with molten gaps, giving Foundry its most visible unique motif.
    for left, top, right, bottom in ((586, 574, 900, 710), (572, 758, 902, 1022)):
        rect((left, top, right, bottom), shadow, ink, 6, 12)
        line(((left + 10, top + 12), (right - 10, top + 12)), orange, 5)
        for x in range(left + 18, right - 12, 34):
            rect((x, top + 24, x + 22, bottom - 14), (73, 78, 82, 255), ink, 3, 5)
            line(((x + 5, top + 30), (x + 17, bottom - 21)), light, 3)
            line(((x + 3, bottom - 19), (x + 19, bottom - 19)), refractory, 5)
        for x in range(left + 43, right - 10, 68):
            line(((x, top + 30), (x, bottom - 20)), orange, 4)

    # Lower wall variants use kiln blocks and asymmetric braces.
    for x, y, height in ((192, 770, 154), (320, 770, 154), (448, 704, 258)):
        line(((x, y), (x, y + height)), light, 5)
        line(((x + 8, y + 12), (x + 50, y + 54)), refractory, 7)
        bolt(x + 12, y + 14, True)
    rect((64, 898, 258, 924), steel, ink, 4, 4)
    for x in range(80, 250, 34):
        line(((x, 903), (x + 18, 919)), shadow, 6)

    details = details.resize((1024, 1024), Image.Resampling.LANCZOS)
    detail_alpha = ImageChops.multiply(details.getchannel("A"), alpha)
    details.putalpha(detail_alpha)
    source = Image.alpha_composite(source, details)
    source.putalpha(alpha)
    source.save(path, optimize=True)


def _hires(size: tuple[int, int]):
    scale = 2
    image = Image.new("RGBA", (size[0] * scale, size[1] * scale), (0, 0, 0, 0))
    return image, ImageDraw.Draw(image), scale


def _save_hires(image: Image.Image, path: Path) -> None:
    image.resize((image.width // 2, image.height // 2), Image.Resampling.LANCZOS).save(path)


def build_doodads_native(path: Path) -> None:
    image, draw, s = _hires((1024, 1024))

    ink = (28, 31, 35, 255)
    shadow = (45, 50, 56, 255)
    steel = (91, 98, 104, 255)
    light = (184, 191, 196, 255)
    orange = (241, 79, 23, 255)
    amber = (255, 177, 52, 255)

    def scaled_box(box):
        return tuple(round(value * s) for value in box)

    def rect(box, fill, outline=None, width=1, radius=3):
        draw.rounded_rectangle(scaled_box(box), radius=radius * s, fill=fill, outline=outline, width=width * s)

    def ellipse(box, fill=None, outline=None, width=1):
        draw.ellipse(scaled_box(box), fill=fill, outline=outline, width=width * s)

    def line(points, fill, width):
        draw.line([(x * s, y * s) for x, y in points], fill=fill, width=width * s, joint="curve")

    def polygon(points, fill):
        draw.polygon([(x * s, y * s) for x, y in points], fill=fill)

    def bolt(x, y, hot=False):
        ellipse((x - 4, y - 4, x + 4, y + 4), ink)
        ellipse((x - 2, y - 2, x + 2, y + 2), amber if hot else light)

    # Required tile 1: a readable alternating chain that remains entirely
    # inside its original 64px atlas slot.
    for link, cy in enumerate(range(3, 61, 14)):
        if link % 2:
            ellipse((88, cy - 2, 102, cy + 14), outline=ink, width=5)
            ellipse((90, cy, 100, cy + 12), outline=light, width=2)
        else:
            ellipse((82, cy, 108, cy + 11), outline=ink, width=5)
            ellipse((85, cy + 2, 105, cy + 9), outline=light, width=2)

    # A large smelter face: layered shell, inspection port, gauge and side
    # manifold. It stays flat and chunky like Ninslash's native doodads.
    rect((8, 304, 302, 554), shadow, ink, 6, 10)
    rect((28, 326, 278, 526), (61, 67, 73, 255), light, 3, 8)
    line(((46, 350), (258, 350)), (215, 219, 221, 255), 3)
    rect((62, 380, 234, 518), (37, 41, 46, 255), ink, 5, 14)
    ellipse((89, 398, 207, 516), fill=(71, 42, 35, 255), outline=ink, width=6)
    ellipse((111, 420, 185, 494), fill=orange, outline=amber, width=5)
    ellipse((196, 334, 258, 396), fill=(54, 60, 66, 255), outline=ink, width=5)
    line(((227, 365), (244, 348)), amber, 4)
    for px, py in ((46, 344), (260, 344), (46, 512), (260, 512)):
        bolt(px, py, px == 260 and py == 512)
    line(((24, 455), (6, 455), (6, 505)), steel, 14)
    line(((24, 451), (6, 451), (6, 501)), light, 3)

    # Overhead crane and magnetic grabber occupy a separate optional region.
    rect((342, 82, 970, 116), steel, ink, 5, 5)
    line(((360, 98), (950, 98)), light, 3)
    for x in range(370, 940, 72):
        line(((x, 112), (x + 42, 145)), shadow, 8)
    rect((572, 102, 700, 154), shadow, ink, 5, 6)
    bolt(592, 128, True)
    bolt(680, 128)
    line(((635, 150), (635, 248)), ink, 8)
    line(((635, 150), (635, 248)), steel, 3)
    polygon(((602, 248), (668, 248), (687, 278), (656, 294), (614, 294), (583, 278)), shadow)
    line(((594, 255), (676, 255)), light, 3)
    rect((609, 265, 661, 286), (64, 36, 31, 255), ink, 3, 5)
    line(((617, 274), (653, 274)), orange, 5)

    # Pressure tanks and a valve manifold give the midground recognisable
    # silhouettes even when only a few doodad tiles are selected.
    for x, height in ((350, 188), (450, 224)):
        rect((x, 330, x + 78, 330 + height), steel, ink, 5, 18)
        rect((x + 10, 348, x + 68, 382), shadow, light, 2, 8)
        line(((x + 16, 398), (x + 62, 398)), (205, 211, 214, 255), 3)
        line(((x + 39, 330), (x + 39, 310)), ink, 10)
        line(((x + 39, 330), (x + 39, 310)), light, 3)
        for y in range(420, 330 + height - 10, 38):
            line(((x + 8, y), (x + 70, y)), shadow, 5)
    line(((528, 388), (610, 388), (610, 510), (690, 510)), ink, 22)
    line(((528, 388), (610, 388), (610, 510), (690, 510)), steel, 13)
    line(((528, 383), (602, 383)), light, 2)
    ellipse((572, 408, 648, 484), fill=shadow, outline=ink, width=5)
    for angle in range(0, 360, 45):
        radians = math.radians(angle)
        line(((610, 446), (610 + math.cos(radians) * 31, 446 + math.sin(radians) * 31)), light, 5)
    ellipse((599, 435, 621, 457), fill=amber, outline=ink, width=4)

    # Required tiles 153-155: one continuous suspended smelter walkway.
    for index in (153, 154, 155):
        tx, ty = (index % 16) * 64, (index // 16) * 64
        rect((tx + 1, ty + 35, tx + 63, ty + 62), steel, ink, 3, 3)
        line(((tx + 6, ty + 39), (tx + 58, ty + 39)), (222, 225, 226, 255), 2)
        line(((tx + 8, ty + 9), (tx + 56, ty + 34)), shadow, 6)
        line(((tx + 56, ty + 9), (tx + 8, ty + 34)), shadow, 6)
        for start in range(tx + 2, tx + 66, 18):
            polygon(((start, ty + 52), (start + 9, ty + 52), (start + 2, ty + 61), (start - 7, ty + 61)), amber if (start // 18) % 2 else ink)

    # Required tiles 169-171: distinct modular foundry crates that still form
    # a coherent three-tile family.
    for offset, index in enumerate((169, 170, 171)):
        tx, ty = (index % 16) * 64, (index // 16) * 64
        rect((tx + 3, ty + 5, tx + 61, ty + 61), steel, ink, 3, 5)
        rect((tx + 9, ty + 12, tx + 55, ty + 42), shadow, light, 2, 4)
        if offset == 0:
            ellipse((tx + 19, ty + 17, tx + 45, ty + 41), fill=(50, 56, 62, 255), outline=ink, width=3)
            line(((tx + 32, ty + 29), (tx + 41, ty + 20)), amber, 3)
        elif offset == 1:
            for slit in (17, 26, 35):
                line(((tx + 14, ty + slit), (tx + 50, ty + slit)), (34, 38, 43, 255), 4)
        else:
            rect((tx + 17, ty + 18, tx + 47, ty + 37), (83, 42, 31, 255), ink, 2, 5)
            line(((tx + 21, ty + 27), (tx + 43, ty + 27)), orange, 5)
        for start in range(tx + 8, tx + 58, 16):
            polygon(((start, ty + 48), (start + 8, ty + 48), (start + 3, ty + 58), (start - 5, ty + 58)), amber if (start // 16) % 2 else ink)

    # Low silhouettes: slag trough, cable coils and small steam vents.
    rect((42, 742, 486, 804), (55, 60, 65, 255), ink, 5, 10)
    polygon(((62, 760), (466, 760), (432, 792), (84, 792)), (103, 49, 34, 255))
    line(((94, 774), (430, 774)), orange, 9)
    line(((104, 770), (420, 770)), amber, 2)
    for x in (570, 650, 730):
        ellipse((x, 750, x + 112, 862), outline=ink, width=13)
        ellipse((x + 14, 764, x + 98, 848), outline=steel, width=8)
    for x in (850, 910):
        rect((x, 742, x + 38, 884), steel, ink, 5, 8)
        line(((x + 19, 742), (x + 19, 706)), ink, 11)
        line(((x + 19, 742), (x + 19, 706)), light, 3)

    _save_hires(image, path)


def build_far_native(path: Path) -> None:
    image, draw, s = _hires((1024, 512))
    for y in range(512 * s):
        t = y / max(1, 512 * s - 1)
        color = (
            round(26 + 25 * t),
            round(29 + 17 * t),
            round(35 + 10 * t),
            255,
        )
        draw.line((0, y, 1024 * s, y), fill=color)

    # A muted back row creates depth without competing with gameplay.
    distant = ((-30, 138, 185), (90, 186, 250), (248, 158, 205), (370, 215, 290), (556, 154, 218), (682, 202, 266), (846, 196, 210))
    for index, (x, width, height) in enumerate(distant):
        top = 512 - height
        draw.rectangle((x * s, top * s, (x + width) * s, 512 * s), fill=(50, 53, 60, 255))
        draw.rectangle(((x + 12) * s, (top + 15) * s, (x + width - 12) * s, (top + 25) * s), fill=(39, 42, 49, 255))
        if index % 2 == 0:
            draw.polygon(((x * s, top * s), ((x + width // 3) * s, (top - 34) * s), ((x + width * 2 // 3) * s, top * s)), fill=(50, 53, 60, 255))
        for px in range(x + 24, x + width - 12, 42):
            draw.rounded_rectangle((px * s, (top + 42) * s, (px + 10) * s, (top + 51) * s), radius=2 * s, fill=(164, 67, 38, 170))

    # Stacks, tanks and catwalks make the silhouette read as a foundry rather
    # than a generic city skyline.
    for x, top, width in ((72, 108, 34), (318, 155, 28), (612, 92, 42), (914, 142, 31)):
        draw.rectangle((x * s, top * s, (x + width) * s, 512 * s), fill=(38, 41, 47, 255))
        draw.rectangle(((x - 6) * s, (top - 10) * s, (x + width + 6) * s, (top + 8) * s), fill=(58, 61, 67, 255))
        for band in range(top + 54, 480, 66):
            draw.rectangle(((x - 3) * s, band * s, (x + width + 3) * s, (band + 6) * s), fill=(62, 65, 71, 255))
    for x, y, radius in ((190, 350, 67), (770, 375, 82)):
        draw.ellipse(((x - radius) * s, (y - radius) * s, (x + radius) * s, (y + radius) * s), fill=(43, 46, 52, 255), outline=(62, 65, 71, 255), width=5 * s)
        draw.rectangle(((x - radius) * s, y * s, (x + radius) * s, 512 * s), fill=(43, 46, 52, 255))
    draw.line((0, 318 * s, 1024 * s, 318 * s), fill=(32, 35, 41, 255), width=13 * s)
    draw.line((0, 314 * s, 1024 * s, 314 * s), fill=(72, 74, 79, 255), width=3 * s)

    haze = Image.new("RGBA", image.size, (0, 0, 0, 0))
    haze_draw = ImageDraw.Draw(haze)
    for box in ((20, 70, 260, 155), (250, 105, 510, 190), (580, 52, 820, 145), (780, 110, 1050, 195)):
        haze_draw.ellipse(tuple(value * s for value in box), fill=(116, 84, 78, 24))
    haze = haze.filter(ImageFilter.GaussianBlur(20 * s))
    image = Image.alpha_composite(image, haze)
    _save_hires(image, path)


def build_glow_native(path: Path) -> None:
    image = Image.new("RGBA", (512, 512), (0, 0, 0, 255))
    px = image.load()
    for y in range(512):
        for x in range(512):
            vertical = y / 511.0
            furnace = max(0.0, 1.0 - (((x - 256) / 290) ** 2 + ((y - 382) / 245) ** 2) ** 0.5)
            side_left = max(0.0, 1.0 - (((x - 70) / 150) ** 2 + ((y - 330) / 220) ** 2) ** 0.5)
            side_right = max(0.0, 1.0 - (((x - 445) / 150) ** 2 + ((y - 345) / 210) ** 2) ** 0.5)
            glow = min(1.0, furnace + (side_left + side_right) * 0.24)
            px[x, y] = (
                int(31 + 24 * vertical + 66 * glow),
                int(31 + 9 * vertical + 19 * glow),
                int(38 + 2 * vertical + 4 * glow),
                255,
            )
    draw = ImageDraw.Draw(image, "RGBA")
    for y in (116, 258, 438):
        draw.rectangle((0, y, 512, y + 5), fill=(20, 23, 28, 74))
        draw.line((0, y, 512, y), fill=(118, 87, 74, 36), width=1)
    image.save(path, optimize=True)


def build_hall_native(path: Path) -> None:
    image, draw, s = _hires((1024, 1024))
    for y in range(1024 * s):
        t = y / max(1, 1024 * s - 1)
        draw.line((0, y, 1024 * s, y), fill=(round(38 + 10 * t), round(41 + 4 * t), round(47 + 1 * t), 255))

    ink = (24, 27, 32, 255)
    shadow = (47, 51, 57, 255)
    steel = (91, 96, 102, 255)
    edge = (145, 151, 156, 255)
    warm = (121, 54, 39, 255)
    orange = (238, 73, 21, 255)
    amber = (255, 159, 47, 255)

    # Four modular bays tile cleanly and stay quieter than the foreground.
    for bay in range(4):
        x = bay * 256
        draw.rectangle((x * s, 0, (x + 22) * s, 1024 * s), fill=ink)
        draw.rectangle(((x + 8) * s, 0, (x + 14) * s, 1024 * s), fill=(57, 61, 67, 255))
        draw.line(((x + 18) * s, 0, (x + 166) * s, 260 * s), fill=shadow, width=13 * s)
        draw.line(((x + 24) * s, 0, (x + 170) * s, 254 * s), fill=(83, 87, 92, 255), width=4 * s)

        # Upper service deck and cable tray.
        draw.rectangle(((x + 22) * s, 260 * s, (x + 256) * s, 304 * s), fill=ink)
        draw.rectangle(((x + 22) * s, 266 * s, (x + 256) * s, 294 * s), fill=steel)
        draw.line(((x + 26) * s, 270 * s, (x + 252) * s, 270 * s), fill=edge, width=3 * s)
        for brace_x in range(x + 40, x + 250, 48):
            draw.line((brace_x * s, 294 * s, (brace_x + 24) * s, 324 * s), fill=shadow, width=7 * s)

        # Furnace shell, recessed door and a restrained molten opening.
        draw.rounded_rectangle(((x + 44) * s, 382 * s, (x + 220) * s, 922 * s), radius=14 * s, fill=ink, outline=edge, width=6 * s)
        draw.rounded_rectangle(((x + 60) * s, 408 * s, (x + 204) * s, 894 * s), radius=10 * s, fill=(34, 37, 42, 255), outline=shadow, width=5 * s)
        draw.polygon((((x + 78) * s, 842 * s), ((x + 186) * s, 842 * s), ((x + 170) * s, 600 * s), ((x + 94) * s, 600 * s)), fill=warm)
        draw.rounded_rectangle(((x + 96) * s, 642 * s, (x + 168) * s, 842 * s), radius=8 * s, fill=orange, outline=(81, 38, 32, 255), width=4 * s)
        draw.rectangle(((x + 103) * s, 651 * s, (x + 161) * s, 664 * s), fill=amber)
        draw.line(((x + 71) * s, 558 * s, (x + 193) * s, 558 * s), fill=edge, width=4 * s)
        for bolt_x in (x + 72, x + 192):
            for bolt_y in (430, 874):
                draw.ellipse(((bolt_x - 5) * s, (bolt_y - 5) * s, (bolt_x + 5) * s, (bolt_y + 5) * s), fill=shadow, outline=ink, width=2 * s)

        # Side manifold and pressure indicator reinforce the machinery scale.
        draw.line(((x + 222) * s, 430 * s, (x + 244) * s, 430 * s, (x + 244) * s, 800 * s), fill=ink, width=20 * s)
        draw.line(((x + 222) * s, 430 * s, (x + 244) * s, 430 * s, (x + 244) * s, 800 * s), fill=steel, width=11 * s)
        draw.ellipse(((x + 213) * s, 460 * s, (x + 251) * s, 498 * s), fill=shadow, outline=ink, width=4 * s)
        draw.line(((x + 232) * s, 479 * s, (x + 242) * s, 469 * s), fill=amber, width=3 * s)

    # Floor trench and heat reflection tie the bays together.
    draw.rectangle((0, 930 * s, 1024 * s, 1024 * s), fill=(26, 29, 34, 255))
    draw.line((0, 936 * s, 1024 * s, 936 * s), fill=(114, 80, 69, 255), width=4 * s)
    for x in range(-20, 1040, 48):
        draw.polygon((x * s, 966 * s, (x + 20) * s, 966 * s, (x - 2) * s, 1008 * s, (x - 22) * s, 1008 * s), fill=(74, 55, 48, 255))
    _save_hires(image, path)


def build_pipes_native(path: Path) -> None:
    image, draw, s = _hires((1024, 512))
    ink = (25, 28, 33, 255)
    steel = (83, 90, 96, 255)
    edge = (169, 177, 182, 255)
    shadow = (48, 53, 59, 255)
    orange = (226, 78, 27, 255)
    amber = (255, 170, 50, 255)

    def pipe(points, width, fill=steel):
        scaled = [(x * s, y * s) for x, y in points]
        draw.line(scaled, fill=ink, width=(width + 12) * s, joint="curve")
        draw.line(scaled, fill=fill, width=width * s, joint="curve")
        highlight = [(x * s, (y - width * 0.18) * s) for x, y in points]
        draw.line(highlight, fill=edge, width=max(2, width // 10) * s, joint="curve")

    def flange(x, y, vertical=False, hot=False):
        if vertical:
            box = ((x - 8) * s, (y - 27) * s, (x + 8) * s, (y + 27) * s)
        else:
            box = ((x - 27) * s, (y - 8) * s, (x + 27) * s, (y + 8) * s)
        draw.rounded_rectangle(box, radius=4 * s, fill=shadow, outline=ink, width=4 * s)
        if hot:
            draw.line(((x - 15) * s, y * s, (x + 15) * s, y * s), fill=orange, width=4 * s)

    pipe(((-24, 88), (238, 88), (304, 154), (566, 154), (638, 88), (1048, 88)), 42)
    for x, y, vertical in ((150, 88, False), (304, 124, True), (480, 154, False), (638, 120, True), (846, 88, False)):
        flange(x, y, vertical, x in (480, 846))

    pipe(((-20, 256), (208, 256), (262, 210), (496, 210), (552, 264), (780, 264), (842, 206), (1044, 206)), 30, (74, 82, 89, 255))
    for x, y in ((104, 256), (372, 210), (666, 264), (920, 206)):
        flange(x, y)

    pipe(((-30, 402), (354, 402), (410, 350), (676, 350), (730, 402), (1050, 402)), 54, (91, 85, 84, 255))
    for x, y in ((128, 402), (516, 350), (874, 402)):
        flange(x, y, hot=x == 516)

    # Vertical branches and two high-readability valve wheels.
    pipe(((262, 210), (262, 348)), 24, (70, 77, 83, 255))
    pipe(((780, 264), (780, 440)), 22, (70, 77, 83, 255))
    for x, y in ((262, 318), (780, 328)):
        draw.ellipse(((x - 35) * s, (y - 35) * s, (x + 35) * s, (y + 35) * s), outline=ink, width=12 * s)
        draw.ellipse(((x - 29) * s, (y - 29) * s, (x + 29) * s, (y + 29) * s), outline=orange, width=5 * s)
        for angle in range(0, 360, 45):
            radians = math.radians(angle)
            draw.line((x * s, y * s, (x + math.cos(radians) * 27) * s, (y + math.sin(radians) * 27) * s), fill=shadow, width=5 * s)
        draw.ellipse(((x - 8) * s, (y - 8) * s, (x + 8) * s, (y + 8) * s), fill=amber, outline=ink, width=3 * s)

    # Small steam gauges and support brackets prevent long featureless runs.
    for x, y in ((710, 74), (584, 250), (950, 388)):
        draw.line((x * s, y * s, x * s, (y - 36) * s), fill=ink, width=8 * s)
        draw.ellipse(((x - 20) * s, (y - 70) * s, (x + 20) * s, (y - 30) * s), fill=shadow, outline=ink, width=4 * s)
        draw.line((x * s, (y - 50) * s, (x + 10) * s, (y - 60) * s), fill=amber, width=3 * s)
    _save_hires(image, path)


def build_smoke_native(path: Path) -> None:
    image, draw, s = _hires((1024, 256))
    ink = (25, 28, 32, 245)
    steel = (84, 91, 97, 255)
    edge = (169, 176, 181, 255)
    orange = (236, 77, 24, 235)
    amber = (255, 176, 55, 235)

    # Overhead rail, trolley and hook are simple silhouettes that remain
    # readable after parallax scaling.
    draw.rectangle((0, 8 * s, 1024 * s, 38 * s), fill=ink)
    draw.rectangle((0, 12 * s, 1024 * s, 30 * s), fill=steel)
    draw.line((0, 14 * s, 1024 * s, 14 * s), fill=edge, width=3 * s)
    for x in range(32, 1024, 96):
        draw.line((x * s, 30 * s, (x + 28) * s, 54 * s), fill=(48, 53, 58, 220), width=7 * s)
    draw.rounded_rectangle((446 * s, 25 * s, 578 * s, 67 * s), radius=7 * s, fill=steel, outline=ink, width=5 * s)
    for x in (468, 556):
        draw.ellipse(((x - 12) * s, 15 * s, (x + 12) * s, 39 * s), fill=(49, 54, 59, 255), outline=ink, width=4 * s)
    draw.line((512 * s, 64 * s, 512 * s, 174 * s), fill=ink, width=9 * s)
    draw.line((512 * s, 64 * s, 512 * s, 174 * s), fill=edge, width=3 * s)
    draw.arc((482 * s, 150 * s, 542 * s, 212 * s), 10, 225, fill=ink, width=10 * s)
    draw.arc((487 * s, 155 * s, 537 * s, 207 * s), 10, 225, fill=orange, width=4 * s)

    # Low-alpha steam masses use overlapping flat lobes instead of blurry
    # photographic clouds. A tiny blur only softens the parallax edge.
    steam = Image.new("RGBA", image.size, (0, 0, 0, 0))
    steam_draw = ImageDraw.Draw(steam)
    plumes = (
        ((-30, 178, 285, 276), (103, 88, 86, 52)),
        ((170, 196, 515, 282), (115, 91, 87, 46)),
        ((610, 170, 910, 270), (111, 88, 86, 50)),
        ((820, 192, 1080, 282), (91, 80, 82, 42)),
    )
    for box, color in plumes:
        left, top, right, bottom = box
        steam_draw.ellipse(tuple(value * s for value in box), fill=color)
        steam_draw.ellipse(((left + 60) * s, (top - 25) * s, (right - 80) * s, (bottom - 8) * s), fill=(color[0] + 8, color[1] + 7, color[2] + 8, max(20, color[3] - 8)))
    steam = steam.filter(ImageFilter.GaussianBlur(3 * s))
    image = Image.alpha_composite(image, steam)
    draw = ImageDraw.Draw(image)

    # Sparse sparks imply active production without creating visual noise.
    for x, y, length in ((92, 164, 12), (168, 202, 8), (332, 170, 10), (704, 192, 9), (888, 166, 13), (952, 212, 7)):
        draw.line((x * s, y * s, (x + 3) * s, (y + length) * s), fill=orange, width=3 * s)
        draw.point(((x + 1) * s, y * s), fill=amber)
    _save_hires(image, path)


def build(root: Path) -> None:
    mapres = root / "data" / "mapres"
    assets = {
        "foundry_main.png": (build_main_tileset_native, (1024, 1024)),
        "foundry_bg_far.png": (build_far_native, (1024, 512)),
        "foundry_bg_glow.png": (build_glow_native, (512, 512)),
        "foundry_parallax_near.png": (build_hall_native, (1024, 1024)),
        "foundry_parallax_mid.png": (build_pipes_native, (1024, 512)),
        "foundry_parallax_smoke.png": (build_smoke_native, (1024, 256)),
        "foundry_doodads.png": (build_doodads_native, (1024, 1024)),
    }
    for filename, (builder, _size) in assets.items():
        builder(mapres / filename)

    template = root / "data" / "maps" / "generate_large3.map"
    output = root / "data" / "maps" / "generate_foundry1.map"
    image_names = write_foundry_map(template, output)

    for filename, (_builder, expected_size) in assets.items():
        width, height, color_type = png_info(mapres / filename)
        if (width, height) != expected_size or color_type != 6:
            raise ValueError(f"invalid generated asset {filename}: {(width, height, color_type)}")
    validate_tileset_correspondence(mapres / "metal_main.png", mapres / "foundry_main.png")
    validate_native_asset_roles(mapres)
    _magic, _header, _fixed, raw_data = read_datafile(output)
    actual_names = [block.rstrip(b"\0").decode("ascii", "replace") for block in raw_data[:7]]
    if actual_names != image_names:
        raise ValueError(f"foundry map image verification failed: {actual_names}")
    if referenced_map_images(output) != set(range(7)):
        raise ValueError(
            f"not all foundry images are referenced: {sorted(referenced_map_images(output))}"
        )
    if sha256(template) == sha256(output):
        raise ValueError("foundry map is still byte-identical to generate_large3.map")

    print("Generated deterministic foundry theme:")
    for filename in assets:
        path = mapres / filename
        print(f"  {path.relative_to(root)}  {sha256(path)[:12]}")
    print(f"  {output.relative_to(root)}  {sha256(output)[:12]}")
    print("External map images: " + ", ".join(image_names))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="repository root (defaults to the script's parent repository)",
    )
    args = parser.parse_args()
    try:
        build(args.root.resolve())
    except (OSError, ValueError, zlib.error) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
