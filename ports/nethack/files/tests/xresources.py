"""Check staged X resources and every SNF glyph against the original BDF."""
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]


def check_font(name, stage):
    data = (stage / 'fonts' / (name + '.snf')).read_bytes()
    header = struct.unpack_from('<16I', data)
    assert header[0] == 4 and struct.unpack_from('<I', data, 104)[0] == 4
    first, last, first_row, last_row = header[7:11]
    assert (first, last, first_row, last_row) == (0, 255, 0, 0)
    glyph_start = 108 + (last - first + 1) * 16
    reverse = bytes(int(f'{i:08b}'[::-1], 2) for i in range(256))
    count = 0
    for block in (ROOT / 'win/X11' / (name + '.bdf')).read_text().split('STARTCHAR ')[1:]:
        lines = block.splitlines()
        encoding = int(next(line.split()[1] for line in lines if line.startswith('ENCODING ')))
        width, height, x, y = map(int, next(line for line in lines if line.startswith('BBX ')).split()[1:])
        advance = int(next(line.split()[1] for line in lines if line.startswith('DWIDTH ')))
        metrics = struct.unpack_from('<5hHI', data, 108 + (encoding - first) * 16)
        assert metrics[:5] == (x, x + width, advance, height + y, -y), (name, encoding, 'metrics')
        assert metrics[6] & (1 << 24), (name, encoding, 'missing glyph')
        offset = glyph_start + (metrics[6] & 0xffffff)
        start = lines.index('BITMAP') + 1
        stride = ((width + 31) // 32) * 4
        expected = b''.join(bytes.fromhex(row).translate(reverse).ljust(stride, b'\0')
                            for row in lines[start:start + height])
        assert data[offset:offset + len(expected)] == expected, (name, encoding, 'bitmap')
        count += 1
    assert count == 256
    # Check that the properties, strings and optional ink metrics fit the file.
    glyph_bytes = struct.unpack_from('<I', data, 92)[0] & 0xffffff
    end = glyph_start + ((glyph_bytes + 3) & ~3) + header[11] * 12 + header[12]
    if header[6] & 8:
        end += (last - first + 3) * 16
    assert len(data) == end, (name, 'SNF length')


def main():
    stage_root = Path(sys.argv[1])
    stage = stage_root / 'local/games/nh313'
    assert (stage_root / 'lib/x11/app-defa/NetHack').read_bytes() == (ROOT / 'win/X11/NetHack.ad').read_bytes()
    assert [p.name for p in stage.iterdir() if p.name.casefold() == 'nethack'] == ['nethack']
    # Check the complete installed tree, not just individual DOS 8.3 names:
    # distinct Linux paths can overwrite each other on a GEMDOS volume.
    dos_paths = set()
    for path in stage_root.rglob('*'):
        relative = path.relative_to(stage_root)
        folded = str(relative).casefold()
        assert folded not in dos_paths, ('GEMDOS filename collision', relative)
        dos_paths.add(folded)
        parts = path.name.split('.')
        assert len(parts) <= 2 and 1 <= len(parts[0]) <= 8, relative
        assert len(parts) == 1 or 1 <= len(parts[1]) <= 3, relative
    assert (stage / 'fonts/fonts.dir').read_text() == '2\nnh10.snf nh10\nibm.snf ibm\n'
    for name in ['nh10', 'ibm']:
        check_font(name, stage)
    print('PASS: GEMDOS-safe package paths, original X resources and all 512 SNF glyphs')


if __name__ == '__main__':
    main()
