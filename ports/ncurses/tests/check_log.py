#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check a terminal.c transcript after keys and at least two window sizes."""
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text()
events = [(int(key), int(cols), int(rows)) for key, cols, rows in
          re.findall(r'key=(\d+) size=(\d+)x(\d+)', text)]
keys = [event[0] for event in events]
checks = {
    'startup': 'startup=PASS' in text,
    'arrows': {258, 259, 260, 261}.issubset(keys),
    'home and end': {262, 360}.issubset(keys),
    'function keys': {265, 266}.issubset(keys),
    'escape then 1': any(keys[i:i + 2] == [27, 49] for i in range(len(keys) - 1)),
    'ordinary text': 106 in keys,
    'resize': len({(c, r) for k, c, r in events if k == 410}) >= 2,
    'clean exit': 'exit=PASS' in text,
}
for name, passed in checks.items():
    print(('PASS ' if passed else 'FAIL ') + name)
sys.exit(not all(checks.values()))
