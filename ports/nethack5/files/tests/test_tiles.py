#!/usr/bin/env python3
"""Check the generated X11 tile file against its 32-bit on-disk layout."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='nh-tiles-') as name:
    binary = Path(name)/'header-check'
    source = '#include "tile2x11.h"\nint main(void) { return sizeof(x11_header) != 24; }\n'
    subprocess.run(['cc', '-x', 'c', '-I'+str(root/'include'), '-', '-o', str(binary)],
                   input=source, text=True, check=True)
    subprocess.run([str(binary)], check=True)
path = Path(sys.argv[1]) if len(sys.argv) > 1 else root/'dat/x11tiles'
data = path.read_bytes()
version, colors, width, height, tiles, columns = struct.unpack_from('<6I', data)
assert version == 2 and 0 < colors <= 256
assert 0 < width <= 256 and 0 < height <= 256 and 0 < columns <= 1024
assert 0 < tiles <= 65536
padded = ((tiles+columns-1)//columns)*columns
assert len(data) == 24+3*colors+padded*width*height, (len(data), padded)
assert max(data[24+3*colors:]) < colors
print(f'PASS 24-byte tile header on this host; {tiles} tiles, {width}x{height}, {colors} colours')
