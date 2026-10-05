#!/usr/bin/env python3
"""Validate the texture-only native-style repaint without altering project files.

Requires Pillow. Run from any directory. Compares against the preserved before
snapshot, then rebuilds all four atlases into a temporary directory and compares
decoded RGBA pixels and metadata bytes. PNG compression may differ across
Pillow/zlib builds; encoded-byte equality is reported separately, not required.
No gameplay, skeleton, atlas-coordinate or alpha changes.
"""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
DESIGN = ROOT / 'design/boss-native-style'
BOSSES = ('skitter_matriarch', 'bastion_strider', 'storm_seraph', 'siege_monolith')


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    report = {'status': 'PASS', 'bosses': {}, 'source_parts': 0}
    baseline = json.loads((DESIGN / 'baseline.json').read_text(encoding='utf8'))
    for rel, expected in baseline['rollback_sha256'].items():
        assert sha(DESIGN / 'rollback' / rel) == expected, 'rollback changed: ' + rel
    with tempfile.TemporaryDirectory(prefix='ninslash-native-style-') as temporary:
        for boss in BOSSES:
            stem = boss.replace('_', '-')
            runtime = ROOT / 'data/anim' / boss
            old = DESIGN / 'rollback/data/anim' / boss
            for ext in ('atlas', 'json'):
                assert (runtime / f'{stem}.{ext}').read_bytes() == (old / f'{stem}.{ext}').read_bytes(), f'{boss}: {ext} changed'
            before = Image.open(old / f'{stem}.png').convert('RGBA')
            after = Image.open(runtime / f'{stem}.png')
            assert after.mode == 'RGBA' and after.size == (1024, 1024), boss + ': invalid atlas format'
            assert before.getchannel('A').tobytes() == after.getchannel('A').tobytes(), boss + ': atlas alpha changed'
            assert before.tobytes() != after.tobytes(), boss + ': atlas not repainted'
            parts = ROOT / 'design/boss-redesign-v5' / boss / 'parts'
            old_parts = DESIGN / 'rollback/design/boss-redesign-v5' / boss / 'parts'
            assert {p.name for p in parts.glob('*.png')} == {p.name for p in old_parts.glob('*.png')}, boss + ': part inventory changed'
            count = 0
            for p in sorted(parts.glob('*.png')):
                original = Image.open(old_parts / p.name).convert('RGBA')
                current = Image.open(p)
                assert current.mode == 'RGBA' and original.size == current.size, str(p) + ': format/size changed'
                assert original.getchannel('A').tobytes() == current.getchannel('A').tobytes(), str(p) + ': alpha changed'
                assert original.tobytes() != current.tobytes(), str(p) + ': not repainted'
                vector = DESIGN / 'source' / boss / (p.stem + '.svg')
                mask = DESIGN / 'source' / boss / 'masks' / p.name
                assert vector.is_file() and mask.is_file(), str(p) + ': missing editable source'
                assert Image.open(mask).tobytes() == current.getchannel('A').tobytes(), str(p) + ': mask mismatch'
                count += 1
            output = Path(temporary) / boss
            if boss == 'skitter_matriarch':
                command = [sys.executable, str(ROOT / 'scripts/build_skitter_matriarch.py'), str(parts), str(output)]
            else:
                command = [sys.executable, str(ROOT / 'scripts/build_boss_v5.py'), boss, str(parts), str(output)]
            subprocess.run(command, check=True, capture_output=True, text=True)
            for ext in ('atlas', 'json'):
                name = f'{stem}.{ext}'
                assert (output / name).read_bytes() == (runtime / name).read_bytes(), f'{boss}: non-reproducible {ext}'
            rebuilt = Image.open(output / f'{stem}.png').convert('RGBA')
            assert rebuilt.size == after.size and rebuilt.tobytes() == after.tobytes(), boss + ': non-reproducible RGBA pixels'
            encoded_equal = sha(output / f'{stem}.png') == sha(runtime / f'{stem}.png')
            report['bosses'][boss] = {'parts': count, 'size': [1024, 1024], 'alpha_unchanged': True,
                                      'metadata_unchanged': True, 'rebuild_pixels_identical': True,
                                      'rebuild_metadata_byte_identical': True, 'png_encoded_byte_identical': encoded_equal,
                                      'png_sha256': sha(runtime / f'{stem}.png')}
            report['source_parts'] += count
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    main()
