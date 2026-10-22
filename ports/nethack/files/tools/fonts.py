"""Build the original NetHack fonts as little-endian Helios X11R4 SNF files."""
import shlex


def build_fonts(root, host, stage, x11, host_cc, run):
    source = x11 / 'fonts/bdftosnf'
    if not (source / 'bdftosnf.c').is_file():
        raise RuntimeError('Missing X11R4 font compiler sources; set --x11-source to the mit directory')
    work = host / 'font-tools'
    work.mkdir(exist_ok=True)
    includes = work / 'X11'
    if includes.is_symlink():
        includes.unlink()
    includes.symlink_to(x11 / 'include', target_is_directory=True)
    sources = [source/name for name in ['bdftosnf.c','fontutil.c']]
    flags = shlex.split(host_cc) + ['-std=gnu89', '-O2',
        '-Wno-implicit-function-declaration', '-Wno-int-conversion',
        '-Wno-incompatible-pointer-types']
    flags += ['-I' + str(p) for p in [work, x11 / 'include',
              x11 / 'server/include', x11 / 'server/ddx/snf', source]]
    # SNF serializes native structures, just like the game data. The build's
    # host ABI check requires an ILP32 little-endian compiler.
    run(flags + sources + ['-o', host / 'bdftosnf'], 'host-bdftosnf')
    fonts = stage / 'fonts'
    fonts.mkdir(exist_ok=True)
    for name in ['nh10', 'ibm']:
        run([host / 'bdftosnf', '-l', '-L', '-p4', '-u1',
             root / 'win/X11' / (name + '.bdf')], 'font-' + name,
            output_file=fonts / (name + '.snf'))
    (fonts / 'fonts.dir').write_text('2\nnh10.snf nh10\nibm.snf ibm\n')
    print('Original nh10 and ibm fonts built for Helios X11R4.', flush=True)
