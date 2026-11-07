#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build the native preview probe using an existing XaoS build's tool settings."""
import argparse
import json
from pathlib import Path
import subprocess


def run(command, log):
    command = list(map(str, command))
    with log.open('w') as output:
        output.write(json.dumps(command) + '\n')
        output.flush()
        subprocess.run(command, stdout=output,
                       stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work', type=Path, required=True,
                        help='Shared ports work directory used to build XaoS')
    args = parser.parse_args()
    port = args.work.resolve() / 'xaos'
    output = port / 'tests'
    output.mkdir(exist_ok=True)
    link = json.loads((port / 'logs/xaos.link.log').read_text().splitlines()[0])
    ui_object = next(Path(word) for word in link if word.endswith('-ui.o'))
    compile_ui = json.loads(ui_object.with_suffix('.compile.log').read_text().splitlines()[0])
    ui_source = next(word for word in compile_ui if word.endswith('/src/ui/ui.c'))

    # Keep the application's GUI support functions, but give the probe main().
    compile_probe = compile_ui.copy()
    compile_probe[compile_probe.index(ui_source)] = str(Path(__file__).with_name('preview.c'))
    compile_probe[compile_probe.index('-o') + 1] = str(output / 'preview.s')
    compile_ui.append('-Dmain=xaos_application_main')
    compile_ui[compile_ui.index('-o') + 1] = str(output / 'application-ui.s')
    run(compile_ui, output / 'application-ui.compile.log')
    run(compile_probe, output / 'preview.compile.log')
    for name in ('application-ui', 'preview'):
        run([link[0], '-p', '-o', output / (name + '.o'), output / (name + '.s')],
            output / (name + '.assemble.log'))

    link[link.index(str(ui_object))] = str(output / 'application-ui.o')
    link.insert(link.index(str(output / 'application-ui.o')), str(output / 'preview.o'))
    link[link.index('-nxaos')] = '-npreview'
    link[link.index('-o') + 1] = str(output / 'preview')
    run(link, output / 'preview.link.log')
    print(output / 'preview')


if __name__ == '__main__':
    main()
