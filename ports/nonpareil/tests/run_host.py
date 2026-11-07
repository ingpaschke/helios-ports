#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the prepared native front end on Linux and exercise all five ROMs."""
import argparse
from pathlib import Path
import os
import subprocess
import sys

PORTS = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(PORTS / "tools"))
import sources


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    source = sources.prepare("nonpareil", args.work)
    config = sources.configuration("nonpareil")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    files = []
    for group in config["build"]["groups"]:
        files += [filename.replace("${src}", str(source)) for filename in group["sources"]]
    command = ["cc", "-std=gnu99", "-O1", "-g", "-DNP_SINGLE_THREAD",
               "-iquote", str(source / "src"), "-iquote", str(source / "helios")]
    if args.sanitize:
        command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    command += files + ["-lexpat", "-lX11", "-lm", "-o", str(output / "nonpar")]
    with (output / "build.log").open("w") as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    subprocess.run([sys.executable, source / "helios/assets.py", source,
                    output / "data"], check=True)
    environment = dict(os.environ)
    if args.sanitize:
        # Upstream leaves simulator allocations to process exit. Check memory
        # access and undefined behaviour without treating that as a new leak.
        environment["ASAN_OPTIONS"] = "detect_leaks=0"
    for model in ("10c", "11c", "12c", "15c", "16c"):
        command = [str(output / "nonpar"), model, "-data", str(output / "data"),
                   "-test", "-state", str(output / (model + ".nst"))]
        result = subprocess.run(command, env=environment, capture_output=True,
                                text=True, timeout=20)
        (output / (model + ".log")).write_text(result.stdout + result.stderr)
        if result.returncode:
            raise RuntimeError(model + " failed; see " + str(output / (model + ".log")))
        print(model + ": ROM arithmetic and state round trip PASS", flush=True)


if __name__ == "__main__":
    main()
