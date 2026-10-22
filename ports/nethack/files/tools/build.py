#!/usr/bin/env python3
"""Build NetHack 3.1.3 and a private Helios/X11 installation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import sys
import tarfile

from fonts import build_fonts

HERE = Path(__file__).resolve().parents[1]
WORK = HERE.parent


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    ap.add_argument('--cpu', choices=['t425', 't800'], default='t425')
    ap.add_argument('--sysroot', type=Path, required=True,
                    help='built Helios SDK containing include/ and lib/')
    ap.add_argument('--clang', default=os.environ.get('CLANG', '/opt/llvm-t800/bin/clang'),
                    help='compiler with the T800 Helios target')
    ap.add_argument('--host-runner', default='qemu-i386')
    ap.add_argument('--runtime', type=Path,
                    default=Path('/opt/llvm-t800/sysroots/helios/lib/llvmrt-generic.lib'),
                    help='Helios LLVM runtime archive')
    ap.add_argument('--asm', type=Path, required=True, help='Helios assembler')
    ap.add_argument('--host-cc', default=os.environ.get('HOST_CC', str(HERE / 'tools/host-cc')))
    ap.add_argument('--x11-source', type=Path, default=WORK / 'x11',
                    help='X11R4 mit source tree containing the bdftosnf font compiler')
    ap.add_argument('--x11-include', type=Path, required=True, help='shared X11 header directory containing X11/')
    ap.add_argument('--x11-library', type=Path, help='rebuilt static X11 library directory')
    args = ap.parse_args()
    root = args.sysroot.resolve()
    out = HERE / 'build' / args.cpu
    out.mkdir(parents=True, exist_ok=True)
    host = HERE / 'build/host'
    host.mkdir(parents=True, exist_ok=True)
    commands = []

    def run(cmd, name, cwd=HERE, output_file=None):
        cmd = list(map(str, cmd))
        if Path(cmd[0]).parent == host:
            cmd = shlex.split(args.host_runner) + cmd
        commands.append(dict(argv=cmd, cwd=str(cwd)))
        (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
        if output_file is None:
            r = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
            log = r.stdout
        else:
            with output_file.open('wb') as output:
                r = subprocess.run(cmd, cwd=cwd, stdout=output, stderr=subprocess.PIPE, text=True, errors='replace')
            log = r.stderr
        (out / (name + '.log')).write_text(log)
        if r.returncode or 'undefined symbol' in log.lower():
            errors = [l for l in log.splitlines() if 'error:' in l.lower() or 'undefined' in l.lower()]
            raise RuntimeError(name + ':\n' + '\n'.join(errors[:12] or log.splitlines()[-12:]))
        return log

    # Use the same headers as the rebuilt X11 libraries.
    inc = args.x11_include.resolve()

    # Host tools must use the same ILP32 little-endian data format as Helios.
    common = ['-std=gnu89', '-DHELIOS', '-I' + str(HERE / 'include'), '-funsigned-char', '-fwrapv',
              '-Wno-implicit-function-declaration', '-Wno-int-conversion', '-Wno-incompatible-pointer-types']
    hc = shlex.split(args.host_cc) + common + ['-DHELIOS_HOST_TOOLS', '-O2']
    check = host / 'abi.c'
    check.write_text('#include <stdio.h>\nint main(void) { unsigned int n=1; return sizeof(int)!=4 || sizeof(long)!=4 || sizeof(void*)!=4 || *(char*)&n!=1; }\n')
    run(hc + [check, '-o', host / 'abi'], 'host-abi-build')
    run([host / 'abi'], 'host-abi-check')
    run(hc + [HERE / s for s in ['util/makedefs.c', 'src/monst.c', 'src/objects.c']] + ['-o', host / 'makedefs'], 'host-makedefs')
    for option in ['o', 'p', 'm', 'z']:
        run([host / 'makedefs', '-' + option], 'makedefs-' + option, HERE / 'util')
    version_key = hashlib.sha256((HERE / 'include/config.h').read_bytes() + (HERE / 'include/heliosconf.h').read_bytes() + (HERE / 'include/patchlevel.h').read_bytes()).hexdigest()
    version_stamp = host / 'version.sha256'
    if not (HERE / 'include/date.h').exists() or not (HERE / 'dat/options').exists() or not version_stamp.exists() or version_stamp.read_text() != version_key:
        run([host / 'makedefs', '-v'], 'makedefs-v', HERE / 'util')
        version_stamp.write_text(version_key)
    for name in ['lev', 'dgn']:
        shutil.copyfile(HERE / 'sys/share' / (name + '_comp.h'), HERE / 'include' / (name + '_comp.h'))
        sources = ['sys/share/' + name + '_yacc.c', 'sys/share/' + name + '_lex.c', 'util/' + name + '_main.c', 'util/panic.c', 'src/alloc.c']
        if name == 'lev':
            sources += ['src/drawing.c', 'src/decl.c', 'src/monst.c', 'src/objects.c']
        run(hc + ['-DWEIRD_LEX=0'] + [HERE / s for s in sources] + ['-o', host / (name + '_comp')], 'host-' + name)
    for option in ['d', 'r', 'q', 'h', 'e']:
        run([host / 'makedefs', '-' + option], 'makedefs-' + option, HERE / 'dat')
    run([host / 'dgn_comp', 'dungeon.pdf'], 'dungeon', HERE / 'dat')
    for src in sorted((HERE / 'dat').glob('*.des')):
        run([host / 'lev_comp', src.name], 'level-' + src.stem, HERE / 'dat')
    print('Host tools and game data built (32-bit little-endian).', flush=True)

    flags = [args.clang, '--target=t800-helios', '-mcpu=' + args.cpu, '--sysroot=' + str(root),
             '-Os', '-S', '-D_BSD', '-I' + str(inc)] + common + ['-Wno-error=return-mismatch',
             '-Wno-error=incompatible-function-pointer-types']
    headers = list((HERE / 'include').glob('*.h')) + list((HERE / 'sys/helios').glob('*.h')) + list(inc.rglob('*.h')) + list((root / 'include').rglob('*.h'))
    compiler_version = subprocess.check_output([args.clang, '--version'], text=True)
    digest = hashlib.sha256(compiler_version.encode() + args.asm.read_bytes() + b''.join(p.read_bytes() for p in sorted(headers))).hexdigest()
    run(hc + [HERE / 'tests/abi.c', '-o', host / 'abi-layout'], 'host-abi-layout')
    layout = run([host / 'abi-layout'], 'abi-layout')
    layout_source = out / 'abi-check.c'
    layout_source.write_text(layout)
    run(flags + ['-fsyntax-only', layout_source], 'target-abi-check')
    print('Target and host binary data layouts match.', flush=True)
    # Test actual target serialization: host parameter widths differ on Helios.
    level_ir = out / 'level-header.ll'
    run(flags + ['-emit-llvm', HERE / 'src/save.c', '-o', level_ir], 'level-header-ir')
    print(run([sys.executable, HERE / 'tests/level-header.py', level_ir],
              'level-header-test').strip(), flush=True)

    objects = []
    def compile_source(src):
        name = str(src.relative_to(HERE)).replace('/', '_')[:-2]
        obj = out / (name + '.o')
        assembly = out / (name + '.s')
        cmd = flags + [src, '-o', assembly]
        key = hashlib.sha256((digest + repr(cmd)).encode() + src.read_bytes()).hexdigest()
        stamp = out / (name + '.sha256')
        if not obj.exists() or not stamp.exists() or stamp.read_text() != key:
            run(cmd, name + '-cc')
            run([args.asm, '-p', '-o', obj, assembly], name + '-asm')
            stamp.write_text(key)
        return obj

    run(hc + ['-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections'] +
        [HERE / p for p in ['tests/port.c', 'src/hacklib.c', 'src/files.c', 'sys/unix/unixunix.c']] +
        ['-o', host / 'port-test'], 'host-port-test-build')
    print(run([host / 'port-test'], 'host-port-test').strip(), flush=True)

    run(hc + ['-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
              '-Wl,--wrap=open,--wrap=close,--wrap=unlink'] +
        [HERE / p for p in ['tests/locking.c', 'src/files.c']] +
        ['-o', host / 'locking-test'], 'host-locking-test-build')
    print(run([host / 'locking-test'], 'host-locking-test').strip(), flush=True)

    sources = sorted((HERE / 'src').glob('*.c'))
    sources += [HERE / s for s in ['sys/unix/unixmain.c', 'sys/unix/unixunix.c', 'sys/share/random.c', 'sys/helios/helios.c']]
    sources += sorted((HERE / 'win/X11').glob('*.c'))
    for src in sources:
        objects.append(compile_source(src))
    runtime = args.runtime
    libs = [root / 'lib' / s for s in ['libxaw.a', 'libxmu.a', 'libxt.a', 'libxext.a', 'libx11.a', 'xlib.def', 'bsd.lib', 'c.lib', 'helios.lib', 'fplib.def']]
    if args.x11_library:
        libs = [args.x11_library / ('lib' + name + '.a') for name in ['Xaw', 'Xmu', 'Xt', 'Xext', 'X11', 'Xau', 'Xdmcp']] + [root / 'lib' / name for name in ['bsd.lib', 'c.lib', 'helios.lib', 'fplib.def']]
    def link(name, objs, libraries):
        run([args.asm, '-o', out / name, '-n' + name, '-s65536', '-h8192', root / 'lib/c0.o'] + objs + ['-l' + str(p) for p in libraries], name + '-link')
    link('nethack', objects, [runtime] + libs)
    link('xprobe', [compile_source(HERE / 'tests/xprobe.c')], [runtime] + libs)
    link('recover', [compile_source(HERE / 'util/recover.c')], [runtime] + libs[-4:])
    stage_root = out / 'stage'
    stage = stage_root / 'local/games/nh313'
    stage.mkdir(parents=True, exist_ok=True)
    # Remove the old resource placement before copying the executable: GEMDOS
    # treats NetHack and nethack as the same filename.
    (stage / 'NetHack').unlink(missing_ok=True)
    (stage / 'save').mkdir(exist_ok=True)
    for name in ['nethack', 'recover']:
        shutil.copyfile(out / name, stage / name)
    for name in ['data', 'rumors', 'quest.dat', 'oracles', 'options', 'dungeon', 'help', 'hh', 'cmdhelp', 'history', 'license', 'opthelp', 'wizhelp']:
        shutil.copyfile(HERE / 'dat' / name, stage / name)
    for src in (HERE / 'dat').glob('*.lev'):
        shutil.copyfile(src, stage / src.name)
    for name in ['perm', 'record', 'logfile']:
        (stage / name).touch(exist_ok=True)
    # Xt finds the application class in Helios's standard app-defaults path.
    app_defaults = stage_root / 'lib/x11/app-defa'
    app_defaults.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(HERE / 'win/X11/NetHack.ad', app_defaults / 'NetHack')
    shutil.copyfile(HERE / 'win/X11/nethack.rc', stage / 'nethack.rc')
    build_fonts(HERE, host, stage, args.x11_source.resolve(), args.host_cc, run)
    for name in ['nethack.cnf', 'README.txt']:
        shutil.copyfile(HERE / 'sys/helios' / name, stage / name)
    print(run([sys.executable, HERE / 'tests/xresources.py', stage_root], 'xresources-test').strip(), flush=True)
    info = dict(compiler=compiler_version.strip(), sysroot=str(root), cpu=args.cpu, objects=len(objects), image_bytes=(out / 'nethack').stat().st_size,
                image_sha256=hashlib.sha256((out / 'nethack').read_bytes()).hexdigest(),
                runtime_validation='Not performed by build; run tools/smoke.py')
    (out / 'build-info.json').write_text(json.dumps(info, indent=2) + '\n')
    manifest = {str(p.relative_to(stage_root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(stage_root.rglob('*')) if p.is_file()}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    dist = HERE / 'dist'
    dist.mkdir(exist_ok=True)
    archive = dist / ('nh313-helios-' + args.cpu + '.tar.gz')
    with tarfile.open(archive, 'w:gz') as tar:
        for directory in ['lib', 'local']:
            tar.add(stage_root / directory, arcname=directory)
    print(archive)
    print(str(stage))


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError) as exc:
        sys.exit(str(exc))
