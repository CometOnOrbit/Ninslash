#!/usr/bin/env python3
"""Build data/anim/skitter_matriarch/skitter-matriarch.{png,atlas,json}.

Inputs are the cut-out painted parts in design/boss-redesign-v5/skitter_matriarch/parts.
The body is drawn part-by-part by the client (procedural legs/body ride, see
src/game/client/components/droids.cpp RenderMatriarch); the Spine file carries the
setup layout so the skelebank pairing and offline tools keep working.
"""
import json, os, sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)  # scripts/ -> repo root
PARTS = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'design', 'boss-redesign-v5', 'skitter_matriarch', 'parts')
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, 'data', 'anim', 'skitter_matriarch')
# name: (target width in atlas px, mirror so the part faces right)
SPEC = {
    'thorax': (400, True), 'abdomen': (260, True), 'sac': (120, True), 'sac_broken': (150, True),
    'head': (200, True), 'mandible': (80, True), 'thigh': (300, False), 'shin': (380, False),
    'hip': (80, False), 'glob': (64, False), 'puddle': (480, False), 'egg': (110, False), 'drop1': (48, False),
}
imgs = {}
for n, (w, m) in SPEC.items():
    im = Image.open(os.path.join(PARTS, n + '.png')).convert('RGBA')
    if m:
        im = im.transpose(Image.FLIP_LEFT_RIGHT)
    h = max(1, round(im.height * w / im.width))
    imgs[n] = im.resize((w, h), Image.LANCZOS)
# shelf packer
W = H = 1024
order = sorted(imgs, key=lambda n: -imgs[n].height)
x = y = 2; shelf = 0; pos = {}
for n in order:
    im = imgs[n]
    if x + im.width + 2 > W:
        x = 2; y += shelf + 2; shelf = 0
    pos[n] = (x, y); x += im.width + 2; shelf = max(shelf, im.height)
assert y + shelf <= H, 'atlas overflow'
page = Image.new('RGBA', (W, H), (0, 0, 0, 0))
for n, (px, py) in pos.items():
    page.alpha_composite(imgs[n], (px, py))
os.makedirs(OUT, exist_ok=True)
page.save(os.path.join(OUT, 'skitter-matriarch.png'), optimize=True)
lines = ['skitter_matriarch/skitter-matriarch.png', f'size: {W},{H}', 'format: RGBA8888',
         'filter: Linear,Linear', 'repeat: none']
for n in sorted(pos):
    px, py = pos[n]; im = imgs[n]
    lines += [n, '  rotate: false', f'  xy: {px}, {py}', f'  size: {im.width}, {im.height}',
              f'  orig: {im.width}, {im.height}', '  offset: 0, 0', '  index: -1']
with open(os.path.join(OUT, 'skitter-matriarch.atlas'), 'w', newline='\n') as f:
    f.write('\n'.join(lines) + '\n')
# Setup layout in world units at render scale 1 (y up, facing right). Must match
# MATRIARCH_* layout constants in src/game/skitter_matriarch.h.
LAYOUT = [  # slot, x, y, w, h
    ('abdomen', -112, 18, 150, 116), ('sac', -150, -26, 58, 80), ('thorax', 8, 8, 236, 98),
    ('head', 120, -2, 112, 79), ('mandible', 150, -30, 34, 60)]
bones = [{'name': 'root'}] + [{'name': s, 'parent': 'root', 'x': x, 'y': y} for s, x, y, _, _ in LAYOUT]
slots = [{'name': s, 'bone': s, 'attachment': s} for s, *_ in LAYOUT]
skins = {'default': {s: {s: {'width': w, 'height': h}} for s, _, _, w, h in LAYOUT}}
anim = {'idle': {'bones': {'thorax': {'translate': [{'time': 0, 'x': 0, 'y': 0}, {'time': 1.2, 'x': 0, 'y': 3},
                                                     {'time': 2.4, 'x': 0, 'y': 0}]}}}}
doc = {'skeleton': {'hash': 'skitter-matriarch-v5', 'spine': '3.6.37', 'width': 360, 'height': 200, 'images': ''},
       'bones': bones, 'slots': slots, 'skins': skins, 'animations': anim}
with open(os.path.join(OUT, 'skitter-matriarch.json'), 'w', newline='\n') as f:
    json.dump(doc, f, separators=(',', ':'))
print('ok', {n: imgs[n].size for n in imgs})
