#!/usr/bin/env python3
"""Resize an xterm through OLWM's four corners on an isolated test display.

Start OLWM and an xterm titled Resize-test at +200+200 first. Requires
libX11, libXtst and xwininfo. Sends real input: never use a shared desktop.
"""
import argparse
import ctypes as C
import re
import subprocess
import time


def run(display):
    x = C.CDLL('libX11.so.6')
    xt = C.CDLL('libXtst.so.6')
    ptr, ulong, integer = C.c_void_p, C.c_ulong, C.c_int
    x.XOpenDisplay.argtypes = [C.c_char_p]
    x.XOpenDisplay.restype = ptr
    x.XCloseDisplay.argtypes = [ptr]
    x.XDefaultRootWindow.argtypes = [ptr]
    x.XDefaultRootWindow.restype = ulong
    x.XSetInputFocus.argtypes = [ptr, ulong, integer, ulong]
    x.XFlush.argtypes = [ptr]
    xt.XTestFakeMotionEvent.argtypes = [ptr, integer, integer, integer, ulong]
    xt.XTestFakeButtonEvent.argtypes = [ptr, C.c_uint, integer, ulong]
    xt.XTestGrabControl.argtypes = [ptr, integer]
    dpy = x.XOpenDisplay(display.encode())
    assert dpy, 'Cannot open test display'
    # Resizing grabs the server; our connection must still send the release.
    xt.XTestGrabControl(dpy, True)
    x.XFlush(dpy)

    def info(window):
        text = subprocess.check_output(
            ['xwininfo', '-display', display, '-id', window, '-stats', '-tree'],
            text=True, timeout=5)
        fields = ['Absolute upper-left X', 'Absolute upper-left Y', 'Width', 'Height']
        geometry = tuple(int(re.search(label + r':\s+(-?\d+)', text)[1])
                         for label in fields)
        parent = re.search(r'Parent window id:\s+(0x[0-9a-f]+)', text)[1]
        return geometry, parent

    def motion(a, b):
        assert xt.XTestFakeMotionEvent(dpy, 0, a, b, 0)
        x.XFlush(dpy)

    def button(down):
        assert xt.XTestFakeButtonEvent(dpy, 1, down, 0)
        x.XFlush(dpy)

    try:
        tree = subprocess.check_output(
            ['xwininfo', '-display', display, '-root', '-tree'], text=True, timeout=5)
        match = re.search(r'(0x[0-9a-f]+) "Resize-test"', tree)
        assert match, 'Start xterm -geometry 40x12+200+200 -title Resize-test first'
        window = match[1]
        _, frame = info(window)
        for focused in [False, True]:
            for corner, sx, sy in [('NW', -1, -1), ('NE', 1, -1),
                                   ('SW', -1, 1), ('SE', 1, 1)]:
                focus = int(window, 16) if focused else x.XDefaultRootWindow(dpy)
                x.XSetInputFocus(dpy, focus, 1, 0)
                x.XFlush(dpy)
                time.sleep(.3)
                before, _ = info(window)
                (fx, fy, fw, fh), _ = info(frame)
                a, b = fx + (fw - 2 if sx > 0 else 1), fy + (fh - 2 if sy > 0 else 1)
                motion(a, b)
                time.sleep(.1)
                button(True)
                try:
                    time.sleep(.1)
                    motion(a + sx * 48, b + sy * 32)
                    time.sleep(.1)
                finally:
                    button(False)
                deadline = time.monotonic() + 5
                while True:
                    after, _ = info(window)
                    if after[2] > before[2] and after[3] > before[3]:
                        break
                    assert time.monotonic() < deadline, (focused, corner, before, after)
                    time.sleep(.05)
                # The opposite corner remains anchored (allow resize increments).
                ax = 0 if sx > 0 else 2
                ay = 0 if sy > 0 else 3
                assert before[0] + (before[2] if ax else 0) == after[0] + (after[2] if ax else 0)
                assert before[1] + (before[3] if ay else 0) == after[1] + (after[3] if ay else 0)
                print('PASS', 'focused' if focused else 'unfocused', corner, before, after)
    finally:
        x.XCloseDisplay(dpy)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--display', required=True)
    run(parser.parse_args().display)
