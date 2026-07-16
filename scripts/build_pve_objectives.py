#!/usr/bin/env python3
"""Build formal bitmap art for Lost Protocol operation objectives."""

from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "data" / "pve_objectives.png"
CELL = 512
SCALE = 2

INK = (18, 20, 23, 255)
DARK = (48, 54, 61, 255)
MID = (92, 101, 110, 255)
LIGHT = (174, 184, 193, 255)
WHITE = (232, 236, 239, 255)
CYAN = (42, 199, 235, 255)
ORANGE = (255, 105, 24, 255)
RED = (244, 46, 35, 255)
GREEN = (53, 205, 112, 255)


def sc(value):
	if isinstance(value, (tuple, list)):
		return tuple(round(v * SCALE) for v in value)
	return round(value * SCALE)


def polygon(draw, points, fill, outline=INK, width=7):
	points = [sc(point) for point in points]
	draw.polygon(points, fill=fill)
	draw.line(points + [points[0]], fill=outline, width=sc(width), joint="curve")


def rounded(draw, box, radius, fill, outline=INK, width=7):
	draw.rounded_rectangle(sc(box), radius=sc(radius), fill=fill, outline=outline, width=sc(width))


def ellipse(draw, box, fill, outline=INK, width=6):
	draw.ellipse(sc(box), fill=fill, outline=outline, width=sc(width))


def line(draw, points, fill, width=6):
	draw.line([sc(point) for point in points], fill=fill, width=sc(width), joint="curve")


def common_base(draw):
	rounded(draw, (102, 420, 410, 458), 12, DARK)
	polygon(draw, ((122, 420), (150, 378), (362, 378), (390, 420)), MID)
	polygon(draw, ((150, 378), (176, 354), (336, 354), (362, 378)), LIGHT)
	line(draw, ((176, 365), (336, 365)), WHITE, 5)
	for x in (145, 367):
		ellipse(draw, (x - 7, 428, x + 7, 442), (28, 31, 35, 255), MID, 3)


def overload_terminal(draw):
	common_base(draw)
	polygon(draw, ((130, 354), (158, 188), (354, 188), (382, 354)), DARK)
	polygon(draw, ((165, 326), (184, 220), (328, 220), (347, 326)), MID)
	polygon(draw, ((188, 300), (201, 235), (311, 235), (324, 300)), (28, 51, 61, 255))
	polygon(draw, ((201, 286), (212, 248), (300, 248), (311, 286)), CYAN, WHITE, 5)
	line(draw, ((220, 268), (246, 268), (258, 258), (290, 258)), (139, 244, 255, 255), 5)
	rounded(draw, (181, 174, 331, 215), 10, DARK)
	rounded(draw, (207, 182, 305, 205), 7, ORANGE, (115, 35, 17, 255), 5)
	for x in (146, 366):
		rounded(draw, (x - 31, 234, x + 31, 338), 12, MID)
		rounded(draw, (x - 13, 257, x + 13, 307), 6, (30, 35, 40, 255), DARK, 4)
		line(draw, ((x - 25, 249), (x + 25, 249)), ORANGE, 5)


def assembly_node(draw):
	common_base(draw)
	rounded(draw, (118, 156, 182, 373), 14, DARK)
	rounded(draw, (330, 156, 394, 373), 14, DARK)
	for x in (150, 362):
		rounded(draw, (x - 17, 182, x + 17, 338), 9, MID, INK, 5)
		line(draw, ((x, 197), (x, 322)), ORANGE, 8)
		for y in (206, 314):
			ellipse(draw, (x - 9, y - 9, x + 9, y + 9), LIGHT, INK, 4)
	polygon(draw, ((108, 168), (134, 128), (378, 128), (404, 168)), MID)
	line(draw, ((148, 143), (364, 143)), WHITE, 5)
	line(draw, ((176, 205), (218, 248)), LIGHT, 16)
	line(draw, ((336, 205), (294, 248)), LIGHT, 16)
	polygon(draw, ((256, 218), (304, 246), (304, 302), (256, 330), (208, 302), (208, 246)), DARK)
	polygon(draw, ((256, 235), (286, 253), (286, 290), (256, 308), (226, 290), (226, 253)), ORANGE, WHITE, 5)
	line(draw, ((242, 265), (270, 265)), (255, 208, 82, 255), 6)


def targeting_beacon(draw):
	common_base(draw)
	polygon(draw, ((174, 390), (234, 315), (278, 315), (338, 390)), DARK)
	rounded(draw, (229, 156, 283, 364), 12, MID)
	line(draw, ((244, 177), (244, 340)), WHITE, 5)
	for y in (208, 252, 296):
		line(draw, ((222, y), (290, y)), DARK, 8)
	polygon(draw, ((256, 80), (326, 150), (256, 220), (186, 150)), DARK)
	polygon(draw, ((256, 101), (305, 150), (256, 199), (207, 150)), RED, WHITE, 5)
	polygon(draw, ((256, 122), (284, 150), (256, 178), (228, 150)), (255, 167, 74, 255), (119, 25, 20, 255), 4)
	line(draw, ((184, 116), (142, 88), (112, 93)), RED, 7)
	line(draw, ((328, 116), (370, 88), (400, 93)), RED, 7)
	ellipse(draw, (99, 80, 123, 104), RED, INK, 4)
	ellipse(draw, (389, 80, 413, 104), RED, INK, 4)


def upload_point(draw):
	common_base(draw)
	polygon(draw, ((156, 354), (174, 172), (338, 172), (356, 354)), DARK)
	polygon(draw, ((184, 326), (195, 204), (317, 204), (328, 326)), MID)
	rounded(draw, (205, 225, 307, 309), 12, (24, 53, 42, 255), INK, 6)
	for y in (284, 260, 236):
		polygon(draw, ((224, y), (256, y - 22), (288, y), (279, y + 9), (256, y - 6), (233, y + 9)), GREEN, WHITE, 3)
	rounded(draw, (197, 154, 315, 196), 10, DARK)
	rounded(draw, (219, 162, 293, 187), 7, GREEN, (22, 99, 57, 255), 5)
	for x in (145, 367):
		rounded(draw, (x - 30, 243, x + 30, 337), 12, MID)
		ellipse(draw, (x - 12, 266, x + 12, 290), GREEN, WHITE, 4)
		line(draw, ((x + (-1 if x < 256 else 1) * 24, 294), (208 if x < 256 else 304, 294)), GREEN, 6)


def main():
	work = Image.new("RGBA", (CELL * 4 * SCALE, CELL * SCALE), (0, 0, 0, 0))
	for index, painter in enumerate((overload_terminal, assembly_node, targeting_beacon, upload_point)):
		cell = Image.new("RGBA", (CELL * SCALE, CELL * SCALE), (0, 0, 0, 0))
		painter(ImageDraw.Draw(cell))
		work.alpha_composite(cell, (index * CELL * SCALE, 0))
	resampling = getattr(Image, "Resampling", Image)
	work.resize((CELL * 4, CELL), resampling.LANCZOS).save(OUT)
	print(f"built {OUT.relative_to(ROOT)}: 4 objective sprites")


if __name__ == "__main__":
	main()
