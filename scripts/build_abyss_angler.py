#!/usr/bin/env python3
"""Build the Abyss Angler boss: vector-painted regions, atlas, Spine 3.6 rig and clips.

    python3 scripts/build_abyss_angler.py [--preview out.png]

Only needs pycairo. Model space is Spine space: units, y up, the fish faces +x.
"""

import argparse
import json
import math
from collections import defaultdict
from pathlib import Path

import cairo

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "data/anim/abyss_angler"
STEM = "abyss-angler"
S = 2.0  # texture pixels per unit
PAD = 3
FPS = 20


def rgb(h, a=1.0):
	h = h.lstrip("#")
	return (int(h[0:2], 16) / 255.0, int(h[2:4], 16) / 255.0, int(h[4:6], 16) / 255.0, a)


def dim(c, k):
	return (c[0] * k, c[1] * k, c[2] * k, c[3])


OUTLINE = rgb("#050b14")
METAL_HI = rgb("#3d6a92")
METAL_MID = rgb("#1c3a5a")
METAL_LO = rgb("#0a1524")
GLOW = (0.36, 0.95, 1.0)


def lin(x0, y0, x1, y1, stops):
	g = cairo.LinearGradient(x0, y0, x1, y1)
	for o, c in stops:
		g.add_color_stop_rgba(o, *c)
	return g


def rad(x, y, r, stops, fx=None, fy=None):
	g = cairo.RadialGradient(x if fx is None else fx, y if fy is None else fy, 0, x, y, r)
	for o, c in stops:
		g.add_color_stop_rgba(o, *c)
	return g


def poly_curve(ctx, pts):
	"""pts: start point then (c1, c2, p) triples."""
	ctx.move_to(*pts[0])
	for c1, c2, p in pts[1:]:
		ctx.curve_to(*c1, *c2, *p)
	ctx.close_path()


def fill_stroke(ctx, path, fill, width=2.4, outline=OUTLINE):
	path(ctx)
	ctx.set_source(fill) if isinstance(fill, cairo.Pattern) else ctx.set_source_rgba(*fill)
	ctx.fill_preserve()
	ctx.set_source_rgba(*outline)
	ctx.set_line_width(width)
	ctx.stroke()


def glow_line(ctx, path, color=GLOW, width=1.6):
	for w, a in ((width * 5, 0.12), (width * 2.6, 0.3), (width, 0.95)):
		path(ctx)
		ctx.set_source_rgba(*color, a)
		ctx.set_line_width(w)
		ctx.stroke()


def glow_dot(ctx, x, y, r, color=GLOW):
	ctx.arc(x, y, r * 2.4, 0, 2 * math.pi)
	ctx.set_source(rad(x, y, r * 2.4, [(0, (*color, 0.45)), (1, (*color, 0))]))
	ctx.fill()
	ctx.arc(x, y, r, 0, 2 * math.pi)
	ctx.set_source(rad(x, y, r, [(0, (1, 1, 1, 1)), (0.5, (*color, 1)), (1, (*color, 0.9))]))
	ctx.fill()


def seam(ctx, pts, clip):
	ctx.save()
	clip(ctx)
	ctx.clip()
	for dy, col, w in ((0, rgb("#071120", 0.85), 2.0), (1.4, (0.55, 0.8, 1.0, 0.22), 1.0)):
		ctx.move_to(pts[0][0], pts[0][1] + dy)
		for c1, c2, p in pts[1:]:
			ctx.curve_to(c1[0], c1[1] + dy, c2[0], c2[1] + dy, p[0], p[1] + dy)
		ctx.set_source_rgba(*col)
		ctx.set_line_width(w)
		ctx.stroke()
	ctx.restore()


def rivet(ctx, x, y, r=2.2):
	ctx.arc(x, y, r, 0, 2 * math.pi)
	ctx.set_source(rad(x - 0.6, y + 0.6, r, [(0, rgb("#9cc3e0")), (1, rgb("#2a4866"))]))
	ctx.fill_preserve()
	ctx.set_source_rgba(*OUTLINE)
	ctx.set_line_width(0.8)
	ctx.stroke()


def rim(ctx, path, dx=2.0, dy=-3.5, color=(0.5, 0.88, 1.0, 0.22), width=5.0):
	ctx.save()
	path(ctx)
	ctx.clip()
	ctx.translate(dx, dy)
	path(ctx)
	ctx.set_source_rgba(*color)
	ctx.set_line_width(width)
	ctx.stroke()
	ctx.restore()


def tooth(ctx, x, y, length, up, width=9.0):
	d = 1 if up else -1
	tip = (x + (-2.0 if up else 1.5), y + d * length)
	ctx.move_to(x - width / 2, y - d * 1.5)
	ctx.curve_to(x - width / 2, y + d * length * 0.4, tip[0] - 1.5, tip[1] - d * length * 0.25, *tip)
	ctx.curve_to(tip[0] + 2.5, tip[1] - d * length * 0.3, x + width / 2, y + d * length * 0.45, x + width / 2, y - d * 1.5)
	ctx.close_path()
	ctx.set_source(lin(x, y, x, tip[1], [(0, rgb("#f3f8f8")), (0.7, rgb("#b9ccd2")), (1, rgb("#7d959d"))]))
	ctx.fill_preserve()
	ctx.set_source_rgba(*rgb("#13202a"))
	ctx.set_line_width(1.3)
	ctx.stroke()
	ctx.move_to(x - width * 0.18, y + d * length * 0.1)
	ctx.line_to(tip[0] - 0.5, tip[1] - d * length * 0.3)
	ctx.set_source_rgba(1, 1, 1, 0.55)
	ctx.set_line_width(0.9)
	ctx.stroke()


# ---------------------------------------------------------------- regions


def body_path(ctx):
	poly_curve(ctx, [(-132, 36),
		((-112, 72), (-80, 102), (-36, 110)),
		((0, 117), (34, 120), (64, 112)),
		((112, 102), (150, 80), (162, 46)),
		((168, 36), (168, 30), (164, 26)),
		((140, 12), (72, 0), (36, -4)),
		((38, -38), (20, -70), (-22, -76)),
		((-70, -82), (-112, -60), (-132, -30)),
		((-138, -10), (-138, 14), (-132, 36))])


def draw_body(ctx):
	fill_stroke(ctx, body_path, lin(0, 120, 0, -82, [(0, METAL_HI), (0.45, METAL_MID), (1, METAL_LO)]), 0)
	ctx.save()
	body_path(ctx)
	ctx.clip()
	ctx.set_source(rad(30, 88, 130, [(0, (0.6, 0.86, 1.0, 0.22)), (1, (0.6, 0.86, 1.0, 0))]))
	ctx.paint()
	ctx.set_source(lin(0, -20, 0, -80, [(0, (0, 0, 0, 0)), (1, (0.0, 0.02, 0.05, 0.45))]))
	ctx.paint()
	ctx.restore()
	for pts in (
		[(-60, 106), ((-76, 40), (-72, -20), (-56, -80))],
		[(8, 118), ((-2, 62), (2, 20), (16, -10))],
		[(-136, 4), ((-80, 16), (-20, 20), (34, 6))],
		[(36, 114), ((58, 84), (100, 74), (156, 62))],
	):
		seam(ctx, pts, body_path)
	for x, y in ((-66, 80), (-71, 40), (-71, 0), (-66, -40), (2, 90), (-1, 50), (4, 14), (-110, 10), (-50, 16)):
		rivet(ctx, x, y)
	# brow plate over the eye
	brow = lambda c: poly_curve(c, [(54, 70), ((70, 86), (96, 88), (110, 74)), ((100, 70), (74, 66), (54, 70))])
	fill_stroke(ctx, brow, lin(0, 88, 0, 66, [(0, rgb("#4b7aa3")), (1, rgb("#1a3552"))]), 1.6)
	# lateral line and belly lights
	glow_line(ctx, lambda c: (c.move_to(-128, -14), c.curve_to(-90, -26, -50, -32, -12, -52)))
	for x, y in ((-104, -42), (-80, -56), (-54, -64), (-28, -68)):
		glow_dot(ctx, x, y, 2.6)
	glow_dot(ctx, 140, 58, 2.0)
	rim(ctx, body_path)
	body_path(ctx)
	ctx.set_source_rgba(*OUTLINE)
	ctx.set_line_width(2.6)
	ctx.stroke()


def maw_path(ctx):
	poly_curve(ctx, [(30, -2),
		((80, 8), (140, 20), (168, 28)),
		((180, 0), (162, -52), (120, -66)),
		((80, -76), (36, -58), (28, -30)),
		((26, -18), (26, -8), (30, -2))])


def draw_maw(ctx):
	fill_stroke(ctx, maw_path, rad(72, -20, 112, [(0, rgb("#c4fdff")), (0.1, rgb("#3fdcef")), (0.32, rgb("#3a0f46")),
		(1, rgb("#0b030f"))]), 2.0)
	ctx.save()
	maw_path(ctx)
	ctx.clip()
	for r in (34, 52, 70, 90):
		ctx.arc(64, -20, r, -1.1, 1.1)
		ctx.set_source_rgba(1.0, 0.45, 0.75, 0.22)
		ctx.set_line_width(2.2)
		ctx.stroke()
	ctx.restore()


def draw_teeth_upper(ctx):
	for x, y, l in ((156, 24, 22), (140, 19, 30), (122, 14, 25), (104, 10, 21), (88, 6, 17), (72, 3, 13), (58, 0, 9)):
		tooth(ctx, x, y, l, False, 8.5)


def jaw_path(ctx):
	poly_curve(ctx, [(36, -4),
		((80, 4), (140, 14), (178, 22)),
		((188, 6), (178, -36), (150, -62)),
		((110, -90), (60, -86), (30, -60)),
		((14, -42), (18, -16), (36, -4))])


def draw_jaw(ctx):
	for x, y, l in ((172, 21, 34), (155, 17, 40), (137, 13, 31), (118, 9, 25), (99, 6, 19), (81, 3, 14), (65, 1, 10)):
		tooth(ctx, x, y, l, True, 9.5)
	fill_stroke(ctx, jaw_path, lin(0, 22, 0, -90, [(0, rgb("#346590")), (0.5, METAL_MID), (1, METAL_LO)]), 0)
	for pts in ([(72, 4), ((66, -24), (62, -56), (58, -84))], [(124, 14), ((122, -20), (120, -56), (112, -84))]):
		seam(ctx, pts, jaw_path)
	for x, y in ((70, -20), (66, -60), (122, -16), (118, -58)):
		rivet(ctx, x, y)
	glow_line(ctx, lambda c: (c.move_to(42, -52), c.curve_to(80, -80, 124, -78, 160, -48)))
	ctx.move_to(146, -64)
	ctx.line_to(140, -84)
	ctx.line_to(132, -70)
	ctx.close_path()
	ctx.set_source(lin(0, -64, 0, -84, [(0, rgb("#c9d7dc")), (1, rgb("#6c8792"))]))
	ctx.fill_preserve()
	ctx.set_source_rgba(*OUTLINE)
	ctx.set_line_width(1.4)
	ctx.stroke()
	rim(ctx, jaw_path, 1.5, -3.0)
	jaw_path(ctx)
	ctx.set_source_rgba(*OUTLINE)
	ctx.set_line_width(2.6)
	ctx.stroke()


def draw_eye(ctx):
	x, y = 78, 50
	ctx.arc(x, y, 17, 0, 2 * math.pi)
	ctx.set_source(rad(x - 4, y + 4, 18, [(0, rgb("#7fa6c6")), (1, rgb("#1a2c40"))]))
	ctx.fill_preserve()
	ctx.set_source_rgba(*OUTLINE)
	ctx.set_line_width(2.0)
	ctx.stroke()
	ctx.arc(x, y, 13, 0, 2 * math.pi)
	ctx.set_source_rgba(*rgb("#04080e"))
	ctx.fill()
	ctx.arc(x, y, 11.5, 0, 2 * math.pi)
	ctx.set_source(rad(x, y, 11.5, [(0, rgb("#fff7c4")), (0.45, rgb("#ffc23d")), (1, rgb("#c04800"))]))
	ctx.fill()
	ctx.save()
	ctx.translate(x + 1.5, y)
	ctx.scale(0.32, 1.0)
	ctx.arc(0, 0, 8.5, 0, 2 * math.pi)
	ctx.restore()
	ctx.set_source_rgba(*rgb("#140500"))
	ctx.fill()
	ctx.arc(x - 4, y + 5, 2.8, 0, 2 * math.pi)
	ctx.set_source_rgba(1, 1, 1, 0.85)
	ctx.fill()


def gill_plate(ctx):
	poly_curve(ctx, [(-60, 36), ((-40, 44), (-14, 30), (-14, 4)), ((-14, -22), (-34, -40), (-58, -34)),
		((-66, -10), (-66, 16), (-60, 36))])


def draw_gill(ctx):
	fill_stroke(ctx, gill_plate, lin(0, 40, 0, -40, [(0, rgb("#26496c")), (1, rgb("#0e1d30"))]), 1.8)
	for i in range(3):
		x = -52 + i * 13
		path = lambda c, x=x: (c.move_to(x - 4, 30), c.curve_to(x + 6, 12, x + 6, -8, x - 2, -28))
		path(ctx)
		ctx.set_source_rgba(*rgb("#03070c"))
		ctx.set_line_width(6)
		ctx.stroke()
		glow_line(ctx, path, width=1.4)


def fin_path(base0, tips, base1, sag=16.0):
	def path(ctx):
		ctx.move_to(*base0)
		prev = base0
		for i, t in enumerate(tips):
			ctx.curve_to((prev[0] * 2 + t[0]) / 3, (prev[1] * 2 + t[1]) / 3, (prev[0] + t[0] * 2) / 3,
				(prev[1] + t[1] * 2) / 3, *t)
			if i + 1 < len(tips):
				n = tips[i + 1]
				mx, my = (t[0] + n[0]) / 2, (t[1] + n[1]) / 2
				bx, by = (base0[0] + base1[0]) / 2, (base0[1] + base1[1]) / 2
				dx, dy = bx - mx, by - my
				dl = math.hypot(dx, dy) or 1
				v = (mx + dx / dl * sag, my + dy / dl * sag)
				ctx.curve_to(t[0], t[1], v[0], v[1], *v)
				ctx.curve_to(v[0], v[1], n[0], n[1], n[0], n[1])
				prev = n
		ctx.curve_to(tips[-1][0], tips[-1][1], base1[0], base1[1], *base1)
		ctx.close_path()
	return path


def draw_fin(ctx, base0, tips, base1, k=1.0, grad=None):
	path = fin_path(base0, tips, base1)
	bx, by = (base0[0] + base1[0]) / 2, (base0[1] + base1[1]) / 2
	tx = sum(t[0] for t in tips) / len(tips)
	ty = sum(t[1] for t in tips) / len(tips)
	fill_stroke(ctx, path, lin(bx, by, tx, ty, [(0, dim(rgb("#17687d"), k)), (1, dim(rgb("#2bb3c6", 0.8), k))]), 0)
	n = len(tips)
	for i, t in enumerate(tips):
		f = i / max(1, n - 1)
		a = (base0[0] + (base1[0] - base0[0]) * f, base0[1] + (base1[1] - base0[1]) * f)
		for w, c in ((4.0, dim(rgb("#061019"), 1)), (2.0, dim(rgb("#9ff4ec"), k))):
			ctx.move_to(*a)
			ctx.line_to(*t)
			ctx.set_source_rgba(*c)
			ctx.set_line_width(w)
			ctx.stroke()
		if k > 0.8:
			glow_dot(ctx, t[0], t[1], 2.0)
	path(ctx)
	ctx.set_source_rgba(*OUTLINE)
	ctx.set_line_width(2.0)
	ctx.stroke()


def draw_fin_dorsal(ctx):
	draw_fin(ctx, (-96, 92), [(-108, 150), (-82, 166), (-54, 168), (-26, 158), (2, 138)], (16, 108))


def draw_fin_pec(ctx, k=1.0):
	draw_fin(ctx, (-4, -34), [(-34, -110), (-60, -112), (-82, -96), (-90, -70)], (-24, -46), k)


def segment_path(x0, h0, x1, h1):
	def path(ctx):
		ctx.move_to(x0, h0)
		ctx.curve_to(x0 + (x1 - x0) * 0.35, h0 + 1, x0 + (x1 - x0) * 0.7, h1 + 4, x1, h1)
		ctx.curve_to(x1 + 6, h1 * 0.5, x1 + 6, -h1 * 0.5, x1, -h1)
		ctx.curve_to(x0 + (x1 - x0) * 0.7, -h1 - 4, x0 + (x1 - x0) * 0.35, -h0 - 1, x0, -h0)
		ctx.close_path()
	return path


def draw_segment(ctx, x0, h0, x1, h1):
	# Tail bones point down -x, so local -y is the fish's back.
	path = segment_path(x0, h0, x1, h1)
	fill_stroke(ctx, path, lin(0, -h0, 0, h0, [(0, METAL_HI), (0.5, METAL_MID), (1, METAL_LO)]), 0)
	for f in (0.4, 0.75):
		x = x0 + (x1 - x0) * f
		h = h0 + (h1 - h0) * f
		seam(ctx, [(x, -h - 2), ((x + 4, -h * 0.4), (x + 4, h * 0.4), (x, h + 2))], path)
		rivet(ctx, x - 4, -h * 0.55, 1.8)
	glow_line(ctx, lambda c: (c.move_to(x0 + 2, 8), c.line_to(x1 - 2, 5)), width=1.4)
	rim(ctx, path, 1.5, 3.0)
	path(ctx)
	ctx.set_source_rgba(*OUTLINE)
	ctx.set_line_width(2.4)
	ctx.stroke()


def tail_fin_path(ctx):
	poly_curve(ctx, [(-4, 14), ((30, 20), (70, 40), (104, 60)), ((90, 40), (74, 16), (60, 0)),
		((74, -16), (90, -40), (104, -60)), ((70, -40), (30, -20), (-4, -14)), ((-6, 0), (-6, 0), (-4, 14))])


def draw_tail_fin(ctx):
	fill_stroke(ctx, tail_fin_path, lin(0, 0, 104, 0, [(0, rgb("#17687d")), (1, rgb("#2bb3c6", 0.85))]), 0)
	for t in ((100, 56), (84, 40), (70, 24), (62, 6), (62, -6), (70, -24), (84, -40), (100, -56)):
		for w, c in ((3.6, rgb("#061019")), (1.8, rgb("#9ff4ec"))):
			ctx.move_to(0, 0)
			ctx.line_to(*t)
			ctx.set_source_rgba(*c)
			ctx.set_line_width(w)
			ctx.stroke()
	glow_line(ctx, lambda c: (c.move_to(70, 40), c.curve_to(84, 50, 96, 56, 104, 60)), width=1.4)
	glow_line(ctx, lambda c: (c.move_to(70, -40), c.curve_to(84, -50, 96, -56, 104, -60)), width=1.4)
	tail_fin_path(ctx)
	ctx.set_source_rgba(*OUTLINE)
	ctx.set_line_width(2.2)
	ctx.stroke()


LURE_LEN = 56.0


def draw_lure_stalk(ctx):
	path = lambda c: (c.new_sub_path(), c.arc(LURE_LEN + 1, 0, 5, -math.pi / 2, math.pi / 2),
		c.arc(-1, 0, 6, math.pi / 2, 3 * math.pi / 2), c.close_path())
	fill_stroke(ctx, path, lin(0, 6, 0, -6, [(0, rgb("#5b86ac")), (0.5, rgb("#22405e")), (1, rgb("#0b1828"))]), 1.6)
	for f in (0.33, 0.66):
		x = LURE_LEN * f
		ctx.move_to(x, -5.5)
		ctx.line_to(x, 5.5)
		ctx.set_source_rgba(*GLOW, 0.9)
		ctx.set_line_width(1.6)
		ctx.stroke()
	ctx.arc(0, 0, 7, 0, 2 * math.pi)
	ctx.set_source(rad(-1, 1, 7, [(0, rgb("#86aed0")), (1, rgb("#24425f"))]))
	ctx.fill_preserve()
	ctx.set_source_rgba(*OUTLINE)
	ctx.set_line_width(1.4)
	ctx.stroke()


def draw_lure_bulb(ctx):
	ctx.move_to(-23, -5)
	ctx.line_to(-12, -9)
	ctx.line_to(-12, 9)
	ctx.line_to(-23, 5)
	ctx.close_path()
	ctx.set_source(lin(0, 9, 0, -9, [(0, rgb("#5b86ac")), (1, rgb("#0f1f33"))]))
	ctx.fill_preserve()
	ctx.set_source_rgba(*OUTLINE)
	ctx.set_line_width(1.4)
	ctx.stroke()
	ctx.arc(0, 0, 15, 0, 2 * math.pi)
	ctx.set_source(rad(0, 0, 15.5, [(0, (1, 1, 1, 1)), (0.3, rgb("#d4feff")), (0.68, rgb("#5ff0ff")),
		(1, rgb("#1fa6cc", 0.95))], -4, 4))
	ctx.fill_preserve()
	ctx.set_source_rgba(*rgb("#0a2a40"))
	ctx.set_line_width(1.6)
	ctx.stroke()
	for sx in (0.42, 0.85):
		ctx.save()
		ctx.scale(sx, 1.0)
		ctx.arc(0, 0, 15, 0, 2 * math.pi)
		ctx.restore()
		ctx.set_source_rgba(*rgb("#1d3c58", 0.9))
		ctx.set_line_width(1.6)
		ctx.stroke()
	ctx.save()
	ctx.translate(-5, 6)
	ctx.scale(1.0, 0.6)
	ctx.arc(0, 0, 4, 0, 2 * math.pi)
	ctx.restore()
	ctx.set_source_rgba(1, 1, 1, 0.9)
	ctx.fill()


def draw_lure_glow(ctx):
	ctx.arc(0, 0, 63, 0, 2 * math.pi)
	ctx.set_source(rad(0, 0, 63, [(0, (0.75, 1.0, 1.0, 0.6)), (0.22, (0.4, 0.92, 1.0, 0.3)), (0.55, (0.25, 0.8, 1.0, 0.1)),
		(1, (0.2, 0.7, 1.0, 0))]))
	ctx.fill()


def draw_bubble(ctx):
	ctx.arc(0, 0, 18, 0, 2 * math.pi)
	ctx.set_source(rad(0, 0, 18, [(0, (0.55, 0.95, 1.0, 0.18)), (0.72, (0.5, 0.92, 1.0, 0.3)), (0.92, (0.75, 1.0, 1.0, 0.85)),
		(1, (0.6, 1.0, 1.0, 0))]))
	ctx.fill()
	glow_dot(ctx, 0, 0, 3.4)
	ctx.arc(0, 0, 13, 1.9, 2.9)
	ctx.set_source_rgba(1, 1, 1, 0.8)
	ctx.set_line_width(2.4)
	ctx.stroke()


# name: (bbox x0, y0, x1, y1 in drawing coords, draw fn, bone, bone origin for model-space art or None)
REGIONS = {
	"body": ((-142, -86, 172, 124), draw_body),
	"maw": ((22, -80, 186, 36), draw_maw),
	"teeth_upper": ((44, -36, 168, 32), draw_teeth_upper),
	"jaw": ((10, -92, 192, 64), draw_jaw),
	"eye": ((58, 30, 98, 70), draw_eye),
	"gill": ((-70, -44, -10, 48), draw_gill),
	"fin_dorsal": ((-114, 84, 24, 174), draw_fin_dorsal),
	"fin_pec": ((-98, -118, 4, -26), draw_fin_pec),
	"fin_pec_far": ((-98, -118, 4, -26), lambda c: draw_fin_pec(c, 0.55)),
	"tail_1": ((-12, -42, 82, 42), lambda c: draw_segment(c, -8, 34, 76, 24)),
	"tail_2": ((-10, -30, 66, 30), lambda c: draw_segment(c, -6, 25, 60, 15)),
	"tail_fin": ((-10, -66, 110, 66), draw_tail_fin),
	"lure_stalk": ((-9, -9, LURE_LEN + 8, 9), draw_lure_stalk),
	"lure_bulb": ((-25, -19, 19, 19), draw_lure_bulb),
	"lure_glow": ((-64, -64, 64, 64), draw_lure_glow),
	"bubble": ((-20, -20, 20, 20), draw_bubble),
}

# name, parent, x, y, rotation, length
BONES = [
	("root", None, 0, 0, 0, 0),
	("hull", "root", 0, 0, 0, 0),
	("jaw", "hull", 34, -16, 0, 0),
	("eye", "hull", 78, 50, 0, 0),
	("gill", "hull", -38, 6, 0, 0),
	("fin_dorsal", "hull", -40, 96, 0, 0),
	("fin_pec", "hull", -10, -38, 0, 0),
	("fin_pec_far", "hull", 6, -30, 0, 0),
	("tail_1", "hull", -128, 4, 180, 70),
	("tail_2", "tail_1", 70, 0, 0, 56),
	("tail_fin", "tail_2", 56, 0, 0, 0),
	("lure_1", "hull", 40, 102, 78, 62),
	("lure_2", "lure_1", 62, 0, -48, 56),
	("lure_3", "lure_2", 56, 0, -60, 46),
	("lure_tip", "lure_3", 46, 0, 0, 0),
	("lure_glow", "lure_tip", 0, 0, 0, 0),
]

# slot, bone, region, model-space art (offset by the bone's setup position) or bone-local art, width override
SLOTS = [
	("fin_pec_far", "fin_pec_far", "fin_pec_far", True, None),
	("fin_dorsal", "fin_dorsal", "fin_dorsal", True, None),
	("tail_fin", "tail_fin", "tail_fin", False, None),
	("tail_2", "tail_2", "tail_2", False, None),
	("tail_1", "tail_1", "tail_1", False, None),
	("maw", "hull", "maw", True, None),
	("body", "hull", "body", True, None),
	("gill", "gill", "gill", True, None),
	("eye", "eye", "eye", True, None),
	("teeth_upper", "hull", "teeth_upper", True, None),
	("jaw", "jaw", "jaw", True, None),
	("fin_pec", "fin_pec", "fin_pec", True, None),
	("lure_1", "lure_1", "lure_stalk", False, 62),
	("lure_2", "lure_2", "lure_stalk", False, 56),
	("lure_3", "lure_3", "lure_stalk", False, 46),
	("lure_glow", "lure_glow", "lure_glow", False, None),
	("lure_bulb", "lure_tip", "lure_bulb", False, None),
]

# The far pectoral fin hangs from a different pivot than its art was drawn for.
MODEL_ART_ORIGIN = {"fin_pec_far": (-10, -38)}


def render_region(name):
	(x0, y0, x1, y1), draw = REGIONS[name]
	w, h = int(math.ceil((x1 - x0) * S)), int(math.ceil((y1 - y0) * S))
	surf = cairo.ImageSurface(cairo.FORMAT_ARGB32, w, h)
	ctx = cairo.Context(surf)
	ctx.set_line_join(cairo.LINE_JOIN_ROUND)
	ctx.set_line_cap(cairo.LINE_CAP_ROUND)
	ctx.scale(S, -S)
	ctx.translate(-x0, -y1)
	draw(ctx)
	return surf


def pack(surfs):
	order = sorted(surfs, key=lambda n: -surfs[n].get_height())
	width, x, y, shelf, places = 1024, PAD, PAD, 0, {}
	for n in order:
		w, h = surfs[n].get_width(), surfs[n].get_height()
		if x + w + PAD > width:
			x, y, shelf = PAD, y + shelf + PAD, 0
		places[n] = (x, y)
		x += w + PAD
		shelf = max(shelf, h)
	height = 1 << (y + shelf + PAD - 1).bit_length()
	page = cairo.ImageSurface(cairo.FORMAT_ARGB32, width, height)
	ctx = cairo.Context(page)
	for n, (px, py) in places.items():
		ctx.set_source_surface(surfs[n], px, py)
		ctx.paint()
	return page, places


# ---------------------------------------------------------------- motion


def smooth(f):
	f = max(0.0, min(1.0, f))
	return f * f * (3 - 2 * f)


def keys(t, pts):
	if t <= pts[0][0]:
		return pts[0][1]
	for (ta, va), (tb, vb) in zip(pts, pts[1:]):
		if t <= tb:
			return va + (vb - va) * smooth((t - ta) / (tb - ta) if tb > ta else 1)
	return pts[-1][1]


def env(t, a, b, c, d):
	return keys(t, [(a, 0.0), (b, 1.0), (c, 1.0), (d, 0.0)])


def sn(t, hz, ph=0.0):
	return math.sin(2 * math.pi * hz * t + ph)


def swim_base(p, t, period, amp=1.0, bob=True):
	hz = 1.0 / period
	if bob:
		p["hull"]["r"] += 2.0 * amp * sn(t, hz)
		p["hull"]["y"] += 5.0 * amp * sn(t, hz, math.pi / 2)
	for bone, a, ph in (("tail_1", 9, -0.7), ("tail_2", 15, -1.4), ("tail_fin", 20, -2.1), ("fin_dorsal", 4, -1.0),
		("lure_1", 4, -0.9), ("lure_2", 7, -1.8), ("lure_3", 10, -2.7)):
		p[bone]["r"] += a * amp * sn(t, hz, ph)
	p["fin_pec"]["r"] += 20 * amp * sn(t, hz) - 4 * amp
	p["fin_pec_far"]["r"] += 16 * amp * sn(t, hz, 0.5) - 4 * amp
	p["lure_glow"]["s"] *= 1 + 0.12 * amp * sn(t, 2 * hz)
	p["jaw"]["r"] += -2.5 * amp - 2.5 * amp * sn(t, hz)
	p["gill"]["sy"] *= 1 + 0.12 * amp * sn(t, hz)


def clip_idle(p, t):
	swim_base(p, t, 2.4, 0.6)


def clip_swim(p, t):
	swim_base(p, t, 1.6)


def clip_bite(p, t):
	swim_base(p, t, 0.65, 0.5, False)
	p["hull"]["r"] += keys(t, [(0, 0), (0.3, 9), (0.42, -7), (0.62, -4), (0.7, -1), (1.3, 0)])
	p["hull"]["x"] += keys(t, [(0, 0), (0.3, -16), (0.5, 30), (0.7, 22), (1.3, 0)])
	p["jaw"]["r"] += keys(t, [(0, 0), (0.3, -36), (0.55, -44), (0.62, -44), (0.68, 4), (0.76, 0)])
	p["lure_1"]["r"] += keys(t, [(0, 0), (0.3, 14), (0.5, -10), (0.8, 4), (1.3, 0)])
	p["lure_2"]["r"] += keys(t, [(0, 0), (0.3, -10), (0.5, 16), (0.8, -4), (1.3, 0)])
	p["lure_3"]["r"] += keys(t, [(0, 0), (0.3, -14), (0.5, 22), (0.85, -5), (1.3, 0)])
	p["tail_1"]["r"] += keys(t, [(0, 0), (0.32, -14), (0.45, 18), (0.7, 0)])
	p["tail_2"]["r"] += keys(t, [(0, 0), (0.34, -18), (0.48, 22), (0.75, 0)])
	p["fin_pec"]["r"] += keys(t, [(0, 0), (0.3, -20), (0.45, 30), (0.8, 0)])
	p["gill"]["sy"] *= 1 + 0.3 * env(t, 0.25, 0.35, 0.6, 0.8)


def clip_lure(p, t):
	swim_base(p, t, 0.8, 0.8, False)
	e = env(t, 0.5, 0.6, 1.9, 2.0)
	p["lure_1"]["r"] += keys(t, [(0, 0), (0.5, 20), (2.0, 20), (2.4, 0)]) + 4 * sn(t, 3) * e
	p["lure_2"]["r"] += keys(t, [(0, 0), (0.5, -14), (2.0, -14), (2.4, 0)]) + 6 * sn(t, 3, -1) * e
	p["lure_3"]["r"] += keys(t, [(0, 0), (0.5, -26), (2.0, -26), (2.4, 0)]) + 8 * sn(t, 3, -2) * e
	p["lure_glow"]["s"] *= keys(t, [(0, 1), (0.5, 1.9), (2.0, 1.9), (2.2, 1.0)]) + 0.25 * sn(t, 6) * e
	p["jaw"]["r"] += keys(t, [(0, 0), (0.6, -50), (2.0, -50), (2.07, 4), (2.15, 0)])
	p["hull"]["r"] += keys(t, [(0, 0), (0.6, -5), (2.0, -5), (2.08, -9), (2.6, 0)])
	p["hull"]["x"] += keys(t, [(0, 0), (0.6, -6), (2.0, -6), (2.1, 20), (2.6, 0)]) + 1.6 * sn(t, 15) * e
	p["hull"]["y"] += 1.2 * sn(t, 13, 1) * e
	p["fin_pec"]["r"] += 22 * sn(t, 2.5) * e
	p["fin_pec_far"]["r"] += 18 * sn(t, 2.5, 0.6) * e
	p["gill"]["sy"] *= 1 + 0.35 * env(t, 0.5, 0.7, 1.9, 2.1)
	p["fin_dorsal"]["r"] += 6 * env(t, 0.4, 0.6, 1.9, 2.2)


SPIT_SHOTS = (0.6, 1.0, 1.4)


def clip_spit(p, t):
	swim_base(p, t, 1.0, 0.7)
	for s in SPIT_SHOTS:
		pulse = keys(t, [(s - 0.14, 0), (s - 0.02, 1), (s + 0.06, 1), (s + 0.2, 0)])
		kick = keys(t, [(s, 0), (s + 0.05, 1), (s + 0.3, 0)])
		p["jaw"]["r"] += -32 * pulse
		p["gill"]["sy"] *= 1 + 0.25 * pulse
		p["hull"]["x"] += -10 * kick
		p["hull"]["r"] += 5 * kick


def clip_dive(p, t):
	e = env(t, 0.62, 0.7, 1.05, 1.15)
	p["hull"]["r"] += keys(t, [(0, 0), (0.45, 24), (0.62, -38), (1.08, -38), (1.15, -46), (1.3, -30), (1.8, 0)])
	p["hull"]["y"] += keys(t, [(0, 0), (0.45, 12), (0.62, 0)])
	p["tail_1"]["r"] += keys(t, [(0, 0), (0.45, -18), (0.62, 10), (1.1, 6), (1.8, 0)]) + 6 * sn(t, 4) * e
	p["tail_2"]["r"] += keys(t, [(0, 0), (0.45, -14), (0.62, 8), (1.8, 0)]) + 9 * sn(t, 4, -1) * e
	p["tail_fin"]["r"] += keys(t, [(0, 0), (0.45, -12), (0.62, 6), (1.8, 0)]) + 12 * sn(t, 4, -2) * e
	p["fin_pec"]["r"] += keys(t, [(0, 0), (0.45, -30), (0.62, 36), (1.1, 36), (1.4, 0)])
	p["fin_pec_far"]["r"] += keys(t, [(0, 0), (0.45, -26), (0.62, 32), (1.1, 32), (1.4, 0)])
	p["fin_dorsal"]["r"] += keys(t, [(0, 0), (0.45, 8), (0.62, -16), (1.1, -16), (1.5, 0)])
	p["lure_1"]["r"] += keys(t, [(0, 0), (0.45, 10), (0.62, -24), (1.1, -24), (1.25, 10), (1.8, 0)])
	p["lure_2"]["r"] += keys(t, [(0, 0), (0.62, -16), (1.1, -16), (1.3, 12), (1.8, 0)])
	p["lure_3"]["r"] += keys(t, [(0, 0), (0.62, -14), (1.1, -14), (1.32, 18), (1.8, 0)])
	p["jaw"]["r"] += keys(t, [(0, 0), (0.5, -8), (0.62, -24), (1.08, -24), (1.14, 3), (1.25, 0)])
	p["lure_glow"]["s"] *= 1 + 0.4 * env(t, 0.4, 0.5, 1.0, 1.2)


def clip_stagger(p, t):
	j = math.sin(min(t / 0.12, 1.0) * math.pi / 2) * math.exp(-5 * max(0.0, t - 0.12))
	w = math.exp(-5 * t) * sn(t, 4)
	p["hull"]["r"] += 14 * j + 3 * w
	p["hull"]["x"] += -16 * j
	p["jaw"]["r"] += -26 * j
	p["lure_1"]["r"] += -18 * j + 8 * w
	p["lure_2"]["r"] += 24 * j - 10 * w
	p["lure_3"]["r"] += -30 * j + 12 * w
	p["tail_1"]["r"] += 12 * w
	p["tail_2"]["r"] += 16 * w
	p["tail_fin"]["r"] += 20 * w
	p["fin_pec"]["r"] += 25 * j
	p["eye"]["s"] *= 1 - 0.2 * j


def clip_roar(p, t):
	e = env(t, 0.35, 0.5, 1.55, 1.8)
	p["hull"]["r"] += keys(t, [(0, 0), (0.4, 8), (1.6, 6), (2.0, 0)]) + 2.5 * sn(t, 18) * e
	p["hull"]["x"] += 1.5 * sn(t, 16) * e
	p["hull"]["y"] += 1.5 * sn(t, 13, 1) * e
	p["jaw"]["r"] += keys(t, [(0, 0), (0.45, -56), (1.6, -56), (1.85, 0)])
	p["lure_glow"]["s"] *= keys(t, [(0, 1), (0.45, 2.3), (1.6, 2.3), (2.0, 1)]) + 0.3 * sn(t, 8) * e
	p["lure_1"]["r"] += 16 * e
	p["lure_2"]["r"] += -8 * e + 5 * sn(t, 7) * e
	p["lure_3"]["r"] += 10 * e + 6 * sn(t, 7, -1) * e
	p["fin_dorsal"]["r"] += 14 * e
	p["fin_pec"]["r"] += -28 * e + 6 * sn(t, 9) * e
	p["fin_pec_far"]["r"] += -24 * e
	p["tail_1"]["r"] += 6 * sn(t, 12) * e
	p["tail_2"]["r"] += 8 * sn(t, 12, -1) * e
	p["tail_fin"]["r"] += -10 * e + 10 * sn(t, 12, -2) * e
	p["gill"]["sy"] *= 1 + 0.4 * e


def clip_death(p, t):
	p["hull"]["r"] += keys(t, [(0, 0), (0.25, 12), (0.5, 8), (2.6, 165), (3.0, 168)])
	p["hull"]["x"] += keys(t, [(0, 0), (0.25, -10), (3.0, -20)])
	p["hull"]["y"] += keys(t, [(0, 0), (0.4, 4), (3.0, -34)])
	p["jaw"]["r"] += keys(t, [(0, 0), (0.25, -42), (0.6, -30), (3.0, -28)])
	p["lure_1"]["r"] += keys(t, [(0, 0), (0.3, -30), (1.5, -55), (3.0, -62)])
	p["lure_2"]["r"] += keys(t, [(0, 0), (0.4, 30), (1.6, -35), (3.0, -40)])
	p["lure_3"]["r"] += keys(t, [(0, 0), (0.5, -20), (1.8, -30), (3.0, -34)])
	flicker = 1 + 0.5 * sn(t, 9) * env(t, 0.6, 0.8, 2.0, 2.4)
	p["lure_glow"]["s"] *= keys(t, [(0, 1), (0.3, 1.6), (0.6, 0.9), (1.6, 0.5), (2.4, 0.12), (3.0, 0.08)]) * flicker
	p["fin_pec"]["r"] += keys(t, [(0, 0), (0.3, -30), (1.5, 30), (3.0, 36)])
	p["fin_pec_far"]["r"] += keys(t, [(0, 0), (0.3, -26), (1.5, 26), (3.0, 30)])
	p["fin_dorsal"]["r"] += keys(t, [(0, 0), (0.3, 12), (2.0, -22), (3.0, -24)])
	p["tail_1"]["r"] += keys(t, [(0, 0), (0.3, -20), (1.0, 14), (3.0, 18)]) + 10 * sn(t, 3) * math.exp(-1.5 * t)
	p["tail_2"]["r"] += keys(t, [(0, 0), (1.2, 12), (3.0, 14)])
	p["tail_fin"]["r"] += keys(t, [(0, 0), (1.4, 10), (3.0, 12)])
	p["eye"]["s"] *= keys(t, [(0, 1), (0.3, 1.15), (2.5, 0.6), (3.0, 0.55)])
	p["gill"]["sy"] *= keys(t, [(0, 1), (0.3, 1.4), (3.0, 1.0)])


# Order and durations must match src/game/abyss_angler.h.
CLIPS = [
	("idle", 2.4, clip_idle),
	("swim", 1.6, clip_swim),
	("bite", 1.3, clip_bite),
	("lure", 2.6, clip_lure),
	("spit", 2.0, clip_spit),
	("dive", 1.8, clip_dive),
	("stagger", 0.8, clip_stagger),
	("roar", 2.0, clip_roar),
	("death", 3.0, clip_death),
]


def blank_pose():
	return defaultdict(lambda: {"r": 0.0, "x": 0.0, "y": 0.0, "s": 1.0, "sy": 1.0})


def sample_pose(fn, t):
	p = blank_pose()
	fn(p, t)
	return p


def bake(name, dur, fn):
	frames = max(2, int(round(dur * FPS)) + 1)
	times = [dur * i / (frames - 1) for i in range(frames)]
	poses = [sample_pose(fn, t) for t in times]
	bones = {}
	for b in [b[0] for b in BONES]:
		tracks = {}
		r = [p[b]["r"] if b in p else 0.0 for p in poses]
		xy = [(p[b]["x"], p[b]["y"]) if b in p else (0.0, 0.0) for p in poses]
		sc = [(p[b]["s"], p[b]["s"] * p[b]["sy"]) if b in p else (1.0, 1.0) for p in poses]
		if any(abs(v) > 1e-3 for v in r):
			tracks["rotate"] = [{"time": round(t, 4), "angle": round(v, 3)} for t, v in zip(times, r)]
		if any(abs(v[0]) > 1e-3 or abs(v[1]) > 1e-3 for v in xy):
			tracks["translate"] = [{"time": round(t, 4), "x": round(v[0], 3), "y": round(v[1], 3)} for t, v in zip(times, xy)]
		if any(abs(v[0] - 1) > 1e-3 or abs(v[1] - 1) > 1e-3 for v in sc):
			tracks["scale"] = [{"time": round(t, 4), "x": round(v[0], 4), "y": round(v[1], 4)} for t, v in zip(times, sc)]
		if tracks:
			bones[b] = tracks
	return {"bones": bones}


# ---------------------------------------------------------------- rig


def mat(r=0.0, x=0.0, y=0.0, sx=1.0, sy=1.0):
	c, s = math.cos(math.radians(r)), math.sin(math.radians(r))
	return (c * sx, s * sx, -s * sy, c * sy, x, y)


def mul(a, b):
	return (a[0] * b[0] + a[2] * b[1], a[1] * b[0] + a[3] * b[1], a[0] * b[2] + a[2] * b[3], a[1] * b[2] + a[3] * b[3],
		a[0] * b[4] + a[2] * b[5] + a[4], a[1] * b[4] + a[3] * b[5] + a[5])


def world_bones(pose=None):
	out = {}
	for name, parent, x, y, r, _ in BONES:
		q = pose[name] if pose is not None and name in pose else {"r": 0, "x": 0, "y": 0, "s": 1, "sy": 1}
		m = mat(r + q["r"], x + q["x"], y + q["y"], q["s"], q["s"] * q["sy"])
		out[name] = mul(out[parent], m) if parent else m
	return out


def attachment(slot):
	name, bone, region, model_art, width = slot
	(x0, y0, x1, y1), _ = REGIONS[region]
	cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
	w, h = x1 - x0, y1 - y0
	if model_art:
		# Model art hangs off unrotated bones whose parents are unrotated, so it only needs an offset.
		ox, oy = MODEL_ART_ORIGIN.get(bone, next((b[2], b[3]) for b in BONES if b[0] == bone))
		return {"type": "region", "x": round(cx - ox, 3), "y": round(cy - oy, 3), "rotation": 0, "width": w, "height": h}
	if width is not None:
		return {"type": "region", "x": round(width / 2, 3), "y": 0, "rotation": 0, "width": width + 16, "height": h}
	return {"type": "region", "x": round(cx, 3), "y": round(cy, 3), "rotation": 0, "width": w, "height": h}


def build_json():
	bones = []
	for name, parent, x, y, r, length in BONES:
		b = {"name": name, "x": x, "y": y, "rotation": r, "length": length}
		if parent:
			b["parent"] = parent
		bones.append(b)
	slots = [{"name": s[0], "bone": s[1], "attachment": s[2]} for s in SLOTS]
	skins = {"default": {s[0]: {s[2]: attachment(s)} for s in SLOTS}}
	anims = {name: bake(name, dur, fn) for name, dur, fn in CLIPS}
	return {"skeleton": {"spine": "3.6.37", "width": 560, "height": 320, "images": "./"}, "bones": bones,
		"slots": slots, "skins": skins, "animations": anims}


# ---------------------------------------------------------------- preview


def draw_rig(ctx, surfs, pose, ox, oy, k):
	wb = world_bones(pose)
	for slot in SLOTS:
		att = attachment(slot)
		img = surfs[slot[2]]
		iw, ih = img.get_width(), img.get_height()
		w, h = att["width"], att["height"]
		a1 = (w / iw, 0, 0, -h / ih, -w / 2, h / 2)
		a2 = mat(att["rotation"], att["x"], att["y"])
		c = (k, 0, 0, -k, ox, oy)
		m = mul(c, mul(wb[slot[1]], mul(a2, a1)))
		ctx.save()
		ctx.transform(cairo.Matrix(*m))
		ctx.set_source_surface(img, 0, 0)
		ctx.get_source().set_filter(cairo.FILTER_GOOD)
		ctx.paint()
		ctx.restore()


def preview(surfs, path):
	cols, k = 5, 0.42
	cw, ch = 300, 230
	rows = len(CLIPS)
	page = cairo.ImageSurface(cairo.FORMAT_ARGB32, cols * cw, rows * ch)
	ctx = cairo.Context(page)
	ctx.set_source(lin(0, 0, 0, rows * ch, [(0, rgb("#0c2a3e")), (1, rgb("#04101c"))]))
	ctx.paint()
	for row, (name, dur, fn) in enumerate(CLIPS):
		for col in range(cols):
			t = dur * col / (cols - 1) * (0.999 if col == cols - 1 else 1)
			draw_rig(ctx, surfs, sample_pose(fn, t), col * cw + 175, row * ch + 130, k)
			ctx.set_source_rgba(0.7, 0.9, 1, 0.8)
			ctx.select_font_face("Sans")
			ctx.set_font_size(13)
			ctx.move_to(col * cw + 8, row * ch + 18)
			ctx.show_text("%s %.2fs" % (name, t))
	page.write_to_png(str(path))


def hit_report():
	wb = world_bones()
	for b in ("hull", "jaw", "eye", "lure_tip", "lure_2", "fin_dorsal", "fin_pec", "tail_1", "tail_2", "tail_fin"):
		m = wb[b]
		print("%-10s %7.1f %7.1f" % (b, m[4], m[5]))


def main():
	ap = argparse.ArgumentParser()
	ap.add_argument("--preview")
	ap.add_argument("--report", action="store_true")
	args = ap.parse_args()
	surfs = {n: render_region(n) for n in REGIONS}
	page, places = pack(surfs)
	OUT.mkdir(parents=True, exist_ok=True)
	page.write_to_png(str(OUT / (STEM + ".png")))
	lines = ["abyss_angler/%s.png" % STEM, "size: %d,%d" % (page.get_width(), page.get_height()), "format: RGBA8888",
		"filter: Linear,Linear", "repeat: none"]
	for n in REGIONS:
		x, y = places[n]
		w, h = surfs[n].get_width(), surfs[n].get_height()
		lines += [n, "  rotate: false", "  xy: %d, %d" % (x, y), "  size: %d, %d" % (w, h), "  orig: %d, %d" % (w, h),
			"  offset: 0, 0", "  index: -1"]
	(OUT / (STEM + ".atlas")).write_text("\n".join(lines) + "\n")
	(OUT / (STEM + ".json")).write_text(json.dumps(build_json(), separators=(",", ":")))
	if args.preview:
		preview(surfs, args.preview)
	if args.report:
		hit_report()


if __name__ == "__main__":
	main()
