#!/usr/bin/env python3
"""Build data/anim/<boss>/<boss-name>.{png,atlas,json} for the v5 bosses.

usage: build_boss_v5.py <bastion_strider|storm_seraph|siege_monolith> [parts_dir] [out_dir]

Inputs are the cut-out painted parts (design/boss-redesign-v5/<boss>/parts). The client
draws every boss part-by-part with procedural animation (IK legs, wing flaps, hover, turret
aim; see src/game/client/components/boss_v5_draw.h). The Spine file only carries the setup
layout so the skelebank pairing and validate_native keep working.
Joint coordinates used by the client are measured on the *unscaled* art, so atlas scaling
here is free; mirroring is applied so every part faces right.
"""
import json, os, sys
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))

# name: (source file, target atlas width, mirror, crop box on the source or None)
BOSSES = {
    'bastion_strider': {
        'art': 'parts_strider',
        'spec': {
            'hull': ('hull', 400, False, None), 'shield': ('shield', 170, False, None),
            'shield_broken': ('shield_broken', 150, False, None), 'mortar': ('mortar', 150, False, None),
            'thruster': ('thruster', 110, False, None), 'foot': ('foot', 130, False, None),
            # thigh = upper part of the full leg painting; shin = the separate lower leg
            'thigh': ('legfull', 160, False, (0, 0, 286, 260)), 'shin': ('thigh', 90, False, None),
            'shell': ('shell', 64, False, None),
        },
        # slot, x, y(down), w, h  -- must match bastion_strider.h client layout
        'layout': [('thruster', -134, -20, 70, 45), ('hull', 0, -10, 250, 153), ('mortar', -46, -100, 96, 100),
                   ('shield', 146, -4, 104, 172)],
    },
    'storm_seraph': {
        'art': 'parts_seraph',
        'spec': {
            'body': ('body', 190, True, None), 'head': ('head', 100, True, None), 'core': ('core', 100, False, None),
            'halo': ('halo', 150, False, None), 'thruster': ('thruster', 76, False, None),
            'node': ('node', 70, False, None), 'orb': ('orb', 96, False, None),
            'wing': ('wing', 360, True, None), 'wing2': ('wing2', 230, True, None),
        },
        'layout': [('wing', -18 - 100, -46, 230, 155), ('thruster', -4, 112, 46, 67), ('body', 0, 0, 112, 175),
                   ('core', 10, -6, 62, 60), ('head', 30, -78, 60, 70), ('halo', 22, -128, 96, 29)],
    },
    'siege_monolith': {
        'art': 'parts_monolith',
        'spec': {
            'hull': ('hull', 230, False, None), 'turret': ('turret', 360, False, None),
            'launcher': ('launcher', 128, False, None), 'thruster': ('thruster', 130, False, None),
            'vent': ('vent', 80, False, None), 'vent_closed': ('vent_closed', 76, False, None),
            'strut': ('strut', 130, False, None), 'bomb': ('bomb', 64, False, None),
        },
        'layout': [('thruster', 0, 150, 82, 96), ('launcher', -40, -150, 80, 50), ('hull', 0, 0, 146, 280),
                   ('turret', 26 + 54, -142, 230, 71)],
    },
}


def main():
    boss = sys.argv[1]
    cfg = BOSSES[boss]
    repo = os.path.dirname(HERE)
    in_repo = os.path.isdir(os.path.join(repo, 'design', 'boss-redesign-v5', boss, 'parts'))
    if len(sys.argv) > 2:
        parts = sys.argv[2]
    elif in_repo:  # scripts/build_boss_v5.py inside the game repo
        parts = os.path.join(repo, 'design', 'boss-redesign-v5', boss, 'parts')
    else:
        parts = os.path.join(HERE, 'art', cfg['art'])
    if len(sys.argv) > 3:
        out = sys.argv[3]
    else:
        out = os.path.join(repo, 'data', 'anim', boss) if in_repo else os.path.join(HERE, 'out', boss)
    stem = boss.replace('_', '-')
    imgs = {}
    for n, (src, w, mirror, crop) in cfg['spec'].items():
        im = Image.open(os.path.join(parts, src + '.png')).convert('RGBA')
        if crop:
            im = im.crop(crop)
        if mirror:
            im = im.transpose(Image.FLIP_LEFT_RIGHT)
        h = max(1, round(im.height * w / im.width))
        imgs[n] = im.resize((w, h), Image.LANCZOS)
    W = H = 1024
    order = sorted(imgs, key=lambda n: -imgs[n].height)
    x = y = 2
    shelf = 0
    pos = {}
    for n in order:
        im = imgs[n]
        if x + im.width + 2 > W:
            x = 2
            y += shelf + 2
            shelf = 0
        pos[n] = (x, y)
        x += im.width + 2
        shelf = max(shelf, im.height)
    assert y + shelf <= H, 'atlas overflow'
    page = Image.new('RGBA', (W, H), (0, 0, 0, 0))
    for n, (px, py) in pos.items():
        page.alpha_composite(imgs[n], (px, py))
    os.makedirs(out, exist_ok=True)
    page.save(os.path.join(out, stem + '.png'), optimize=True)
    lines = [f'{boss}/{stem}.png', f'size: {W},{H}', 'format: RGBA8888', 'filter: Linear,Linear', 'repeat: none']
    for n in sorted(pos):
        px, py = pos[n]
        im = imgs[n]
        lines += [n, '  rotate: false', f'  xy: {px}, {py}', f'  size: {im.width}, {im.height}',
                  f'  orig: {im.width}, {im.height}', '  offset: 0, 0', '  index: -1']
    with open(os.path.join(out, stem + '.atlas'), 'w', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    layout = cfg['layout']
    root = layout[[s for s, *_ in layout].index(next(s for s, *_ in layout if s in ('hull', 'body')))][0]
    bones = [{'name': 'root'}] + [{'name': s, 'parent': 'root', 'x': x, 'y': -y} for s, x, y, _, _ in layout]
    slots = [{'name': s, 'bone': s, 'attachment': s} for s, *_ in layout]
    skins = {'default': {s: {s: {'width': w, 'height': h}} for s, _, _, w, h in layout}}
    anim = {'idle': {'bones': {root: {'translate': [{'time': 0, 'x': 0, 'y': 0}, {'time': 1.2, 'x': 0, 'y': 3},
                                                    {'time': 2.4, 'x': 0, 'y': 0}]}}}}
    doc = {'skeleton': {'hash': stem + '-v5', 'spine': '3.6.37', 'width': 400, 'height': 320, 'images': ''},
           'bones': bones, 'slots': slots, 'skins': skins, 'animations': anim}
    with open(os.path.join(out, stem + '.json'), 'w', newline='\n') as f:
        json.dump(doc, f, separators=(',', ':'))
    print('ok', boss, {n: imgs[n].size for n in imgs})


if __name__ == '__main__':
    main()
