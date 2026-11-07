#!/usr/bin/env python3
"""Check desktop exposure handling on a dedicated, running X test server."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--display", required=True, help="dedicated X test display")
    parser.add_argument("--cc", default="cc", help="host C compiler")
    parser.add_argument("--sanitize", action="store_true", help="enable ASan and UBSan")
    parser.add_argument("--external", action="store_true",
                        help="check an already running cached renderer instead")
    args = parser.parse_args()
    port = Path(__file__).resolve().parent.parent
    source = port / "files" / "helios"
    with tempfile.TemporaryDirectory(prefix="fvwm-desktop-") as temporary:
        executable = Path(temporary) / "desktop-test"
        command = [args.cc, "-std=c89", "-pedantic", "-Wall", "-Wextra", "-Werror", "-g"]
        if args.sanitize:
            command.append("-fsanitize=address,undefined")
        command.extend([
            f"-I{source}", str(port / "tests" / "desktop.c"), str(source / "desktop.c"),
            "-Wl,--wrap=XDrawArc,--wrap=XFillRectangles,--wrap=XDrawString,"
            "--wrap=XCreatePixmap,--wrap=time",
            "-lX11", "-o", str(executable),
        ])
        subprocess.run(command, check=True)
        environment = dict(os.environ, DISPLAY=args.display)
        run = [str(executable)]
        if args.external:
            run.append("--external")
        subprocess.run(run, check=True, env=environment)


if __name__ == "__main__":
    main()
