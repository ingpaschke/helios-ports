#!/usr/bin/env python3
"""Embed the upstream default configuration as twm's fallback resource."""
from pathlib import Path

lines = Path('system.twmrc').read_text().splitlines()
with Path('deftwmrc.c').open('w') as out:
    out.write('/* Generated from system.twmrc. */\nchar *defTwmrc[] = {\n')
    for line in lines:
        if not line.startswith('#'):
            out.write('    "' + line.replace('"', '\\"') + '",\n')
    out.write('    (char *) 0 };\n')
