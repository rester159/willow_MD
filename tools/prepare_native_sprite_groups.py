#!/usr/bin/env python3
"""Prepare larger Genesis sprite resources without changing source geometry.

This first resource assessment uses verified frame1200 templates. It does not
freeze these captured instances into a live renderer or claim frame-rate gains.
"""
from functools import lru_cache
from pathlib import Path
import hashlib
import json
import shutil
import struct

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'build/cartridge-gap-audit/frame1200_window_v1'
PLAN = ROOT / 'build/native-prepared-objects/final1200/prepared-plan.json'
OUT = ROOT / 'build/native-sprite-groups/frame1200'


def digest(data):
    return hashlib.sha256(data).hexdigest()


def unpack_cell(vram, attribute):
    """Decode original resident 16x16 sprite with Genesis column-major tiles."""
    image = [[0]*16 for _ in range(16)]
    base = attribute & 2047
    for y in range(16):
        for x in range(16):
            at = (base+(x//8)*2+y//8)*32+(y%8)*4+(x%8)//2
            pen = (vram[at] >> (0 if x&1 else 4)) & 15
            dx = 15-x if attribute & 0x0800 else x
            dy = 15-y if attribute & 0x1000 else y
            image[dy][dx] = pen
    return image


def pack(image):
    """Compile final native column-major 8x8 patterns; local flips are baked."""
    height, width = len(image), len(image[0])
    data = bytearray()
    for tx in range(width//8):
        for ty in range(height//8):
            for y in range(8):
                for x in range(0, 8, 2):
                    data.append((image[ty*8+y][tx*8+x] << 4) | image[ty*8+y][tx*8+x+1])
    return data


def candidates(template):
    cells = template['cells']
    width = ((template['attribute'] >> 8) & 15)+1
    height = (template['attribute'] >> 12)+1
    grid = {(c['offset'] & 255, c['offset'] >> 8): (n, c) for n, c in enumerate(cells)}
    options = {}
    for y in range(height):
        for x in range(width):
            for h, w in [(1, 1), (1, 2), (2, 1), (2, 2)]:
                # Stay within the original shape's footprint. Transparent or
                # different-page children can be blank in this group's atlas.
                if x+w > width or y+h > height:
                    continue
                members = [grid[(xx*16, yy*16)] for yy in range(y, y+h) for xx in range(x, x+w)
                           if (xx*16, yy*16) in grid]
                for page in {c['attribute'] & 0xe000 for _, c in members}:
                    covered = [n for n, c in members if c['attribute'] & 0xe000 == page]
                    mask = sum(1 << n for n in covered)
                    if not mask:
                        continue
                    candidate = {'x': x*16, 'y': y*16, 'width': w*16, 'height': h*16,
                                 'page_priority': page, 'members': covered, 'mask': mask}
                    cost = (w*h, y, x, h, w)
                    if mask not in options or cost < options[mask][0]:
                        options[mask] = (cost, candidate)
    return [candidate for _, candidate in options.values()]


def group_template(template, vram):
    options = candidates(template)
    cells = template['cells']
    # Exact minimum-entry cover for this small captured template. No opaque
    # source cell is duplicated or dropped. Tie-break by total fetched area.
    by_cell = [[c for c in options if c['mask'] & (1 << n)] for n in range(len(cells))]

    @lru_cache(None)
    def solve(remaining):
        if not remaining:
            return (0, 0), ()
        first = (remaining & -remaining).bit_length()-1
        best = None
        for c in by_cell[first]:
            if c['mask'] & remaining != c['mask']:
                continue
            cost, result = solve(remaining ^ c['mask'])
            candidate = ((cost[0]+1, cost[1]+c['width']*c['height']), (c,)+result)
            if best is None or candidate[0] < best[0]:
                best = candidate
        assert best is not None
        return best

    _, groups = solve((1 << len(cells))-1)
    result = []
    for group in sorted(groups, key=lambda c: min(c['members'])):
        group = dict(group)
        w, h = group['width'], group['height']
        pens = [[0]*w for _ in range(h)]
        owners = [[None]*w for _ in range(h)]
        for n in group['members']:
            cell = cells[n]
            image = unpack_cell(vram, cell['attribute'])
            x = (cell['offset'] & 255)-group['x']
            y = (cell['offset'] >> 8)-group['y']
            for py in range(16):
                for px in range(16):
                    pens[y+py][x+px] = image[py][px]
                    if image[py][px]:
                        owners[y+py][x+px] = (n, py, px)
        result.append((group, pens, owners))
    return result


def paint(canvas, image, x, y, ordinal):
    for row, values in enumerate(image):
        dy = y+row
        if not 0 <= dy < 224:
            continue
        for col, value in enumerate(values):
            dx = x+col
            if value is not None and 0 <= dx < 320:
                canvas[dy*320+dx] = (ordinal, value)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    source_manifest = json.loads((SOURCE / 'manifest.json').read_text())
    for name, info in source_manifest['buffers'].items():
        blob = (SOURCE / name).read_bytes()
        assert len(blob) == info['bytes'] and digest(blob) == info['sha256']
    plan = json.loads(PLAN.read_text())
    assert plan['view'] == [96, 16, 320, 224, 0]
    vram = (SOURCE / 'vram.bin').read_bytes()
    all_groups = [group_template(t, vram) for t in plan['templates']]
    original_owner, grouped_owner = [None]*(320*224), [None]*(320*224)
    # First entry has source-derived front priority. Paint reverse order.
    for instance in reversed(plan['instances']):
        template = plan['templates'][instance['template']]
        for n, cell in reversed(list(enumerate(template['cells']))):
            pens = unpack_cell(vram, cell['attribute'])
            owners = [[(n, y, x) if pens[y][x] else None for x in range(16)] for y in range(16)]
            x = ((instance['source_x']+(cell['offset'] & 255)) & 511)-96
            y = ((instance['source_y']+(cell['offset'] >> 8)) & 511)-16
            paint(original_owner, owners, x, y, instance['ordinal'])
        for group, pens, owners in reversed(all_groups[instance['template']]):
            # A merged block must not wrap source512px boundaries. The live
            # renderer needs a16px fallback when this per-instance proof fails.
            sx = (instance['source_x']+group['x']) & 511
            sy = (instance['source_y']+group['y']) & 511
            assert sx+group['width'] <= 512 and sy+group['height'] <= 512
            paint(grouped_owner, owners, sx-96, sy-16, instance['ordinal'])
    assert original_owner == grouped_owner

    first_tile = next(o['first_tile'] for o in source_manifest['ownership'] if o['kind'] == 'objects')
    data = bytearray()
    output_templates = []
    for groups in all_groups:
        compiled = []
        for group, pens, owners in groups:
            group = dict(group)
            group['tile'] = first_tile+len(data)//32
            group['attribute'] = group['tile'] | group['page_priority']
            group['size_word'] = ((group['width']//8-1) << 10) | ((group['height']//8-1) << 8)
            group['pattern_bytes'] = len(pack(pens))
            data.extend(pack(pens))
            compiled.append(group)
        output_templates.append(compiled)
    new_vram = bytearray(vram)
    old_end = (source_manifest['highest_pattern_tile']+1)*32
    new_end = first_tile*32+len(data)
    assert new_end <= 0xc000
    new_vram[first_tile*32:max(old_end, new_end)] = bytes(max(old_end, new_end)-first_tile*32)
    new_vram[first_tile*32:new_end] = data
    new_vram[0xf400:0xf680] = bytes(640)
    sat = []
    counts, pixels = [0]*224, [0]*224
    for instance in plan['instances']:
        for group in output_templates[instance['template']]:
            x = ((instance['source_x']+group['x']) & 511)-96
            y = ((instance['source_y']+group['y']) & 511)-16
            if x+group['width'] <= 0 or x >= 320 or y+group['height'] <= 0 or y >= 224:
                continue
            for line in range(max(0, y), min(224, y+group['height'])):
                counts[line] += 1
                pixels[line] += group['width']
            sat.append([y+128, group['size_word'], group['attribute'], x+128])
    assert len(sat) <= 80 and max(counts) <= 20 and max(pixels) <= 320
    for n, entry in enumerate(sat):
        entry[1] |= n+1 if n+1 < len(sat) else 0
        struct.pack_into('>4H', new_vram, 0xf400+n*8, *entry)
    (OUT / 'vram.bin').write_bytes(new_vram)
    for filename in ['cram.bin', 'vsram.bin', 'registers.bin', source_manifest['expected_image']]:
        shutil.copy2(SOURCE / filename, OUT / filename)
    manifest = dict(source_manifest)
    manifest.update(scope='Static resource grouping assessment; original geometry labels and source-derived fitted pixels preserved. '
                    'Prebuilt grouped SAT is diagnostic input, not a native runtime geometry or FPS proof.',
                    sat_entries=len(sat), max_line_sprites=max(counts), max_line_pixels=max(pixels),
                    highest_pattern_tile=first_tile+len(data)//32-1)
    manifest['ownership'] = [dict(o, tiles=len(data)//32) if o['kind'] == 'objects' else o for o in manifest['ownership']]
    manifest['source_geometry'] = 'Original16px child geometry compiled into same-page native16/32px sprites, with opaque-cell ownership checked exactly'
    manifest['buffers'] = {name: {'bytes': (OUT/name).stat().st_size, 'sha256': digest((OUT/name).read_bytes())}
                           for name in source_manifest['buffers']}
    (OUT / 'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    report = {'scope': manifest['scope'], 'source_scene': 1200,
              'old_sat_entries': source_manifest['sat_entries'], 'new_sat_entries': len(sat),
              'old_object_patterns': (old_end-first_tile*32)//32, 'new_object_patterns': len(data)//32,
              'original_cell_ownership_pixels_checked': len(original_owner),
              'original_cell_ownership_mismatches': 0,
              'max_line_sprites': max(counts), 'max_line_pixels': max(pixels),
              'both_player_pages_unchanged': (OUT/'cram.bin').read_bytes() == (SOURCE/'cram.bin').read_bytes(),
              'templates': output_templates, 'instances': plan['instances'],
              'live_requirements': ['Source-driven animation/topology/residency changes must prepare new groups',
                                    'No-wrap instance check or16px fallback before publication',
                                    'Global flip and moving/clipped instances need their own native geometry parity gate',
                                    'Width-specific scanline fetch budgets include transparent padding',
                                    'Actual target pixel check still required'],
              'hashes': {str(p): digest(p.read_bytes()) for p in [Path(__file__).resolve(), PLAN, SOURCE/'manifest.json', SOURCE/'vram.bin']}}
    (OUT / 'group-plan.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ['templates', 'instances', 'hashes']}, indent=2))


if __name__ == '__main__':
    main()
