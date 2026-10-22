#!/usr/bin/env python3
"""Boot a private Helios tree and exercise the X11 game, including save/restore."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

HERE = Path(__file__).resolve().parents[1]
WORK = HERE.parent


def check_image(path):
    data = path.read_bytes()
    if len(data) < 100:
        raise ValueError('truncated XWD: ' + str(path))
    h = struct.unpack_from('>25I', data)
    size, version, fmt, depth, width, height = h[:6]
    if (version, fmt, depth, width, height, h[11]) != (7, 2, 8, 640, 480, 8):
        raise ValueError('unexpected XWD format: ' + str(path))
    start = size + h[19] * 12
    if size < 100 or len(data) != start + h[12] * height:
        raise ValueError('incomplete XWD: ' + str(path))
    pixels = data[start:]
    # A window tree alone does not prove rendering. Require substantial ink.
    counts = [pixels.count(i) for i in set(pixels)]
    if len(counts) < 2 or sorted(counts)[-2] < 500:
        raise ValueError('XWD has no substantial rendered content: ' + str(path))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--cpu', choices=['t425', 't800'], default='t425')
    ap.add_argument('--sysroot', type=Path, default=WORK / 't4-root')
    ap.add_argument('--server', type=Path)
    ap.add_argument('--runner', type=Path, default=WORK / 'ioserver-linux/helios-run')
    ap.add_argument('--template', type=Path, default=WORK / 'ioserver-linux/host.con')
    ap.add_argument('--memory-mb', type=int, choices=range(4, 9), default=8)
    ap.add_argument('--stage-only', action='store_true')
    ap.add_argument('--check-only', action='store_true')
    args = ap.parse_args()
    out = HERE / 'build' / args.cpu
    emu = out / 'smoke'
    root = emu / 'root'
    game = root / 'local/games/nh313'
    probe = root / 'users/root/nhsmoke'
    report = emu / 'report.json'
    transcript = emu / 'console.txt'
    server = args.server or WORK / 'xserver/build' / args.cpu / 'server/xhelios'
    guest = '/helios/local/games/nh313'
    test = '/helios/users/root/nhsmoke'
    commands = [
        'setenv DISPLAY helios:9',
        'setenv NETHACKOPTIONS time,!news',
        guest + '/nethack -s',
        test + '/xhelios :9 notcp > ' + test + '/server.log & echo server-launched',
        'sleep 2',
        '/helios/bin/x11/xdpyinfo',
        # This isolated display bypasses STARTX, so reproduce its font setup.
        '/helios/bin/x11/xset fp+ /helios/lib/x11/fonts/75dpi,' + guest + '/fonts',
        '/helios/bin/x11/xlsfonts -fn nh10',
        guest + '/nethack -u Smoke-V > ' + test + '/game1.log & echo game-launched',
        'sleep 4',
        test + '/xprobe space',
        '/helios/bin/x11/xwininfo -root -tree',
        '/helios/bin/x11/xwd -root -out ' + test + '/new.xwd',
        test + '/xprobe period period',
        test + '/xprobe S y space',
        'sleep 2',
        test + '/xprobe',
        'cp ' + guest + '/save/0Smoke ' + test + '/first.sav',
        guest + '/nethack -u Smoke-V > ' + test + '/game2.log & echo restore-launched',
        'sleep 4',
        test + '/xprobe space',
        '/helios/bin/x11/xwd -root -out ' + test + '/restored.xwd',
        test + '/xprobe period',
        test + '/xprobe S y space',
        'sleep 2',
        test + '/xprobe',
        'cp ' + guest + '/save/0Smoke ' + test + '/second.sav',
    ]
    (emu).mkdir(parents=True, exist_ok=True)
    (emu / 'commands.txt').write_text('\n'.join(commands) + '\n')
    if not args.check_only:
        if not root.exists():
            # Materialize rather than share SDK symlinks with a running guest.
            shutil.copytree(args.sysroot, root, symlinks=False,
                                ignore=lambda path, names: [n for n in names if (Path(path) / n).is_symlink()])
        for dest, source in [(game, out / 'stage/local/games/nh313')]:
            if dest.exists():
                shutil.rmtree(dest)
            shutil.copytree(source, dest)
        app_defaults = root / 'lib/x11/app-defa'
        app_defaults.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(out / 'stage/lib/x11/app-defa/NetHack', app_defaults / 'NetHack')
        if probe.exists():
            shutil.rmtree(probe)
        probe.mkdir(parents=True)
        shutil.copyfile(out / 'xprobe', probe / 'xprobe')
        shutil.copyfile(server, probe / 'xhelios')
        conf = args.template.read_text()
        conf = re.sub(r'(?m)^helios_directory\s*=.*$', 'helios_directory = ' + str(root), conf)
        conf = re.sub(r'(?m)^transputer_memory\s*=.*$', 'transputer_memory = ' + hex(args.memory_mb * 1024 * 1024), conf)
        (emu / 'host.con').write_text(conf)
        if args.stage_only:
            print(emu)
            return 0
        for p in [report, transcript]:
            p.unlink(missing_ok=True)
        env = dict(os.environ, HELIOS_RUN_CON=str(emu / 'host.con'))
        r = subprocess.run([sys.executable, str(args.runner), '-s4' if args.cpu == 't425' else '-s8',
                            '-t', '40', '-T', '300', '--json', str(report),
                            '--transcript', str(transcript)] + commands, env=env)
        if r.returncode:
            print('Runtime test did not complete; inspect ' + str(transcript), file=sys.stderr)
            return 1
    results = json.loads(report.read_text())['results']
    failures = []
    if len(results) != len(commands):
        failures.append('incomplete command sequence')
    for i, (command, result) in enumerate(zip(commands, results)):
        status = result.get('status')
        output = result.get('output', '')
        expected = [0, '0']
        if command.endswith('/xprobe'):
            expected = [3, '3']
            if 'NetHack input window missing' not in output:
                failures.append('game did not exit after saving')
        elif '/xwininfo ' in command:
            expected += [1, '1']  # legacy xwininfo falls off main after success
            if 'nethack' not in output.lower():
                failures.append('NetHack window absent from X tree')
        if result.get('cmd') != command or status not in expected:
            failures.append('command %d: unexpected status %r' % (i + 1, status))
        if 'xdpyinfo' in command and 'Helios' not in output:
            failures.append('display probe did not identify Helios')
        if 'xlsfonts -fn nh10' in command and 'nh10' not in output.split():
            failures.append('NetHack nh10 font unavailable')
    for name in ['new.xwd', 'restored.xwd']:
        try:
            check_image(probe / name)
        except (OSError, ValueError) as e:
            failures.append(str(e))
    saves = [probe / 'first.sav', probe / 'second.sav']
    if any(not p.exists() or p.stat().st_size < 100 for p in saves):
        failures.append('save/restore did not produce both nonempty saves')
    elif saves[0].read_bytes() == saves[1].read_bytes():
        failures.append('saved game did not change after restored play')
    for name in ['game1.log', 'game2.log']:
        p = probe / name
        if p.exists() and re.search(r'panic|cannot|error|failed|unable to load.*font', p.read_text(errors='replace'), re.I):
            failures.append('game diagnostics in ' + str(p))
    validation = dict(passed=not failures, failures=failures, cpu=args.cpu,
                      memory_mb=args.memory_mb, report=str(report))
    (emu / 'validation.json').write_text(json.dumps(validation, indent=2) + '\n')
    if failures:
        print('\n'.join('FAIL: ' + f for f in failures), file=sys.stderr)
        return 1
    print('PASS: Helios X11 startup, rendering, keyboard events, save, restore and resave')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError) as e:
        sys.exit(str(e))
