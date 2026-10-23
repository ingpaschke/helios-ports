#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build native regression probes from an existing XView stage."""
import argparse
import json
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
for program in ('adapters', 'chooser', 'events', 'terminal'):
    for number, template in enumerate(commands):
        command = [str(part).replace('adapters', program) for part in template]
        log = out/(program+'-'+str(number)+'.log')
        with log.open('w') as output:
            result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT)
        if result.returncode:
            raise SystemExit('Build failed; see '+str(log))
    print(out/program)

# Recompile the actual clock with only its startup allocator redirected, then
# link the hook against the same libraries as the production program.
clock_link = json.loads((work/'xview/logs/olclock.link.log').read_text().splitlines()[0])
clock_object = next(Path(p) for p in clock_link if '/obj/clock/' in p and p.endswith('.o'))
clock_compile = json.loads(clock_object.with_suffix('.compile.log').read_text().splitlines()[0])
clock_compile[0] = str(args.toolchain/'bin/clang')
clock_compile[clock_compile.index('-o')+1] = str(out/'clock-oom.s')
clock_compile.append('-Dcalloc=clock_test_calloc')
hook_compile = [str(p).replace('adapters','fail_alloc') for p in commands[0]]
clock_link[0] = str(args.asm)
clock_link[clock_link.index('-nolclock')] = '-nclock-oom'
clock_link[clock_link.index('-o')+1] = str(out/'clock-oom')
clock_link[clock_link.index(str(clock_object))] = str(out/'clock-oom.o')
clock_link.insert(clock_link.index(str(out/'clock-oom.o'))+1,str(out/'fail_alloc.o'))
checks = [clock_compile,hook_compile,
          [args.asm,'-p','-o',out/'clock-oom.o',out/'clock-oom.s'],
          [args.asm,'-p','-o',out/'fail_alloc.o',out/'fail_alloc.s'],clock_link]
for number, command in enumerate(checks):
    log = out/('clock-oom-'+str(number)+'.log')
    with log.open('w') as output:
        result = subprocess.run(list(map(str,command)),stdout=output,stderr=subprocess.STDOUT)
    if result.returncode:
        raise SystemExit('Build failed; see '+str(log))
print(out/'clock-oom')
