#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check resize recovery on a private X display with a 16 MiB Helios client."""
import argparse
import json
from pathlib import Path
import sys
import time

# Reuse the repository's Xlib/XTest helpers without injecting desktop input.
sys.path.insert(0, str(Path(__file__).resolve().parents[2] /
                      'xterm/files/helios/tests'))
from x11 import Desktop, wait_for


def check_resizes(desktop, window, pump=lambda: None, settle_seconds=4):
    """The oversized pair needs 24 MiB, exceeding the client's entire RAM."""
    requests = [(800, 600), (320, 240), (1024, 768), (640, 480),
                (2048, 1536), (480, 360), (640, 480),
                (2048, 1536), (320, 240), (800, 600)]
    results = []
    for width, height in requests:
        desktop.resize(window, width, height)
        deadline = time.monotonic() + settle_seconds
        while time.monotonic() < deadline:
            pump()
            assert desktop.find('XaoS') == window, 'XaoS exited during resize'
            time.sleep(.05)
        expected = (640, 480) if width == 2048 else (width, height)
        def redrawn():
            pump()
            assert desktop.find('XaoS') == window, 'XaoS exited during resize'
            attributes = desktop.attrs(window)
            actual = (attributes.width, attributes.height)
            if actual != expected:
                return False
            pixels = desktop.image(*actual, window=window)
            colours = pixels.getcolors(maxcolors=1000000)
            return colours is None or len(colours) > 8
        wait_for(redrawn, timeout=20)
        result = {'requested': [width, height], 'actual': list(expected),
                  'redrawn': True}
        results.append(result)
        print(json.dumps(result), flush=True)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--display', required=True, help='Private test X display')
    parser.add_argument('--output', type=Path, help='Optional JSON results file')
    parser.add_argument('--settle-seconds', type=float, default=4,
                        help='Wait for each resize to be processed and rendered')
    args = parser.parse_args()
    desktop = Desktop(args.display)
    _, window = wait_for(lambda: desktop.find('XaoS'), timeout=40)
    results = check_resizes(desktop, window, settle_seconds=args.settle_seconds)
    if args.output:
        args.output.write_text(json.dumps(results, indent=2) + '\n')


if __name__ == '__main__':
    main()
