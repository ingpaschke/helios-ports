#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the native adapter regression program from an existing port stage."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--work', type=Path, required=True, help='Shared ports work directory')
parser.add_argument('--sdk', type=Path, required=True)
parser.add_argument('--asm', type=Path, required=True)
parser.add_argument('--compat', type=Path, required=True)
parser.add_argument('--toolchain', type=Path, default=Path('/opt/llvm-t800'))
args = parser.parse_args()
work, sdk = args.work.resolve(), args.sdk.resolve()
out = work/'xview/tests'
out.mkdir(parents=True, exist_ok=True)
source = Path(__file__).with_name('adapters.c').resolve()
commands = [
    [args.toolchain/'bin/clang', '--target=t800-helios', '-mcpu=t800',
     '--sysroot='+str(sdk), '-Dhelios', '-D_BSD',
     '-I'+str(work/'xview/stage/local/include'), '-I'+str(work/'x11/include'), '-Os', '-std=gnu89',
     '-S', source, '-o', out/'adapters.s'],
    [args.asm, '-p', '-o', out/'adapters.o', out/'adapters.s'],
    [args.asm, '-nadapters', '-s65536', '-h131072', '-o', out/'adapters',
     sdk/'lib/c0.o', out/'adapters.o',
     '-l'+str(work/'xview/stage/local/lib/xview.lib'),
     '-l'+str(work/'regexp/stage/local/lib/regexp.lib'),
     '-l'+str(work/'olgx/stage/local/lib/olgx.lib'),
     '-l'+str(work/'x11/lib/libXext.a'), '-l'+str(work/'x11/lib/libX11.a'),
     '-l'+str(args.compat.resolve()/'lib/helios-unix.lib'),
     *['-l'+str(sdk/'lib'/n) for n in ('netutils.lib','bsd.lib','c.lib','helios.lib')],
     '-l'+str(args.toolchain/'sysroots/helios/lib/llvmrt-generic.lib')],
]
for program in ('adapters', 'chooser'):
    for number, template in enumerate(commands):
        command = [str(part).replace('adapters', program) for part in template]
        log = out/(program+'-'+str(number)+'.log')
        with log.open('w') as output:
            result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT)
        if result.returncode:
            raise SystemExit('Build failed; see '+str(log))
    print(out/program)
