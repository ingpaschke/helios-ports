#!/usr/bin/env python3
"""Check OLWM dragging on an isolated X server with Calculator and xmahjongg.

Requires Python 3, libX11, libXtst and xwininfo on the test host. Start the
native manager and both clients first. --solid also checks the first motion
before release; it requires OpenWindows.DragWindow: True in that manager.
This sends real input; never point it at a desktop somebody is using.
"""
import argparse
import ctypes as C
import re
import subprocess
import time


def run(display, solid, start_timeout):
    x = C.CDLL('libX11.so.6')
    xt = C.CDLL('libXtst.so.6')
    ptr, ulong, integer = C.c_void_p, C.c_ulong, C.c_int
    x.XOpenDisplay.argtypes = [C.c_char_p]
    x.XOpenDisplay.restype = ptr
    x.XCloseDisplay.argtypes = [ptr]
    x.XRaiseWindow.argtypes = [ptr, ulong]
    x.XSetInputFocus.argtypes = [ptr, ulong, integer, ulong]
    x.XFlush.argtypes = [ptr]
    xt.XTestFakeMotionEvent.argtypes = [ptr, integer, integer, integer, ulong]
    xt.XTestFakeButtonEvent.argtypes = [ptr, C.c_uint, integer, ulong]
    xt.XTestGrabControl.argtypes = [ptr, integer]
    dpy = x.XOpenDisplay(display.encode())
    assert dpy, 'Cannot open test display'
    # Outline drags grab the server. Keep this test connection able to send
    # the release that ends the drag and to observe its result.
    xt.XTestGrabControl(dpy, True)
    x.XFlush(dpy)

    def position(window):
        result = subprocess.check_output(
            ['xwininfo', '-display', display, '-id', window], text=True)
        return tuple(int(re.search(r'Absolute upper-left ' + axis +
                                   r':\s+(-?\d+)', result)[1])
                     for axis in ['X', 'Y'])

    def motion(a, b):
        assert xt.XTestFakeMotionEvent(dpy, 0, a, b, 0)
        x.XFlush(dpy)

    def button(down):
        assert xt.XTestFakeButtonEvent(dpy, 1, down, 0)
        x.XFlush(dpy)

    def wait_position(window, expected, timeout=5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            actual = position(window)
            if actual == expected:
                return
            time.sleep(.05)
        raise AssertionError((window, 'expected', expected, 'actual', actual))

    try:
        tree = subprocess.check_output(
            ['xwininfo', '-display', display, '-root', '-tree'], text=True)
        windows = {}
        for title in ['Calculator', 'xmahjongg']:
            match = re.search(r'(0x[0-9a-f]+) "' + title + r'"', tree)
            assert match, 'Start ' + title + ' on the test display first'
            windows[title] = match[1]
        for title, window in windows.items():
            other = next(value for value in windows.values() if value != window)
            for steps in [[8], [8, 16], [8, 16, 32, 64]]:
                # An unfocused passive client has synchronous focus grabs.
                x.XSetInputFocus(dpy, int(other, 16), 1, 0)
                x.XRaiseWindow(dpy, int(window, 16))
                x.XFlush(dpy)
                time.sleep(1)  # Allow the native manager to process the raise.
                before = position(window)
                a, b = before[0] + 100, before[1] - 12
                motion(a, b)
                time.sleep(.2)
                button(True)
                try:
                    time.sleep(.01)
                    for delta in steps:
                        motion(a + delta, b + delta)
                        if solid:
                            wait_position(window, (before[0] + delta,
                                                   before[1] + delta), start_timeout)
                        else:
                            time.sleep(.1)
                finally:
                    button(False)
                wait_position(window, (before[0] + steps[-1],
                                       before[1] + steps[-1]))
                print(title, steps, 'PASS', flush=True)
    finally:
        button(False)
        x.XCloseDisplay(dpy)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--display', required=True,
                        help='isolated test display, never the active desktop')
    parser.add_argument('--solid', action='store_true',
                        help='also verify initial motion before releasing')
    parser.add_argument('--max-start-delay', type=float, default=5,
                        help='seconds allowed for solid moves to respond')
    args = parser.parse_args()
    if args.max_start_delay <= 0:
        parser.error('--max-start-delay must be positive')
    run(args.display, args.solid, args.max_start_delay)
