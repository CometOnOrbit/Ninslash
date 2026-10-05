#!/usr/bin/env python3
"""Optional SVG -> existing editable PNG parts. Does not change any runtime atlas.

Dependencies: Pillow, CairoSVG (and Cairo on Windows). Normal atlas rebuilds
need only Pillow and the existing build_boss_v5/build_skitter_matriarch scripts.
All dimensions and alpha masks are fixed by the original sprite footprints.
"""
import argparse
import io
from pathlib import Path
import cairosvg
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
BOSSES = ('skitter_matriarch', 'bastion_strider', 'storm_seraph', 'siege_monolith')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True, help='Fresh output directory; never silently overwrites current parts.')
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Output already exists. Use a new directory and review results before replacing parts.')
    for boss in BOSSES:
        source = ROOT / 'design/boss-native-style/source' / boss
        destination = args.output / boss / 'parts'
        destination.mkdir(parents=True)
        for path in sorted(source.glob('*.svg')):
            mask = Image.open(source / 'masks' / (path.stem + '.png')).convert('L')
            w, h = mask.size
            raw = cairosvg.svg2png(bytestring=path.read_bytes(), output_width=w * 2, output_height=h * 2)
            rendered = Image.open(io.BytesIO(raw)).convert('RGBA').resize((w, h), Image.Resampling.LANCZOS)
            effect = boss == 'storm_seraph' and path.stem in ('halo', 'orb')
            image = Image.new('RGBA', (w, h), '#24bde8' if effect else '#090b0d')
            image.alpha_composite(rendered)
            image.putalpha(mask)
            image.save(destination / (path.stem + '.png'), optimize=True)
    print('Rasterized 41 parts. Review first, then use the existing atlas builders.')


if __name__ == '__main__':
    main()
