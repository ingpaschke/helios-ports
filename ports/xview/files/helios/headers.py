#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Lay out XView headers using the public/private lists in its Imakefiles."""
from pathlib import Path
import re
import shutil
import sys

source, destination = map(Path, sys.argv[1:])
for imake in sorted((source / 'lib/libxview').glob('*/Imakefile')):
    text = imake.read_text().replace('\\\n', ' ')
    for variable, subdir in [('HDRSPUBLIC', 'xview'), ('HDRSPRIVATE', 'xview_private')]:
        match = re.search(r'^' + variable + r'\s*=([^\n]*)', text, re.M)
        if not match:
            continue
        target = destination / subdir
        target.mkdir(parents=True, exist_ok=True)
        for name in match[1].split():
            if not name.endswith('.h'):
                continue
            header = imake.parent / name
            if not header.exists():
                raise SystemExit('Missing upstream header: ' + str(header))
            shutil.copy2(header, target / name)
target = destination / 'pixrect'
target.mkdir(parents=True, exist_ok=True)
for header in sorted((source / 'lib/pixrect').glob('*.h')):
    shutil.copy2(header, target / header.name)
shutil.copy2(source / 'lib/pixrect/rasterfile.h', destination / 'rasterfile.h')

shutil.copy2(source / "helios/notify-time.h", destination / "xview/helios.h")
