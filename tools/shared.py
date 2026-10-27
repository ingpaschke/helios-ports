# SPDX-License-Identifier: MIT
"""Build native Helios resident libraries with an explicit, stable interface.

Public function slots and data sizes come from a readable JSON interface.
Private C data follows the interface and remains private to each task.
"""
import json
from pathlib import Path
import re
import struct


def private_symbols(assembly, functions, prefix):
    """C files formerly had separate modules; keep their static names separate."""
    public = set(re.findall(r'^\s*global\s+(_[\w.$]+)\s*$', assembly, re.M))
    symbols = set(re.findall(r'^([.][A-Za-z_$][\w.$]*):', assembly, re.M))
    symbols -= {'.' + name for name in functions}
    symbols.update(set(re.findall(r'^\s*data\s+(_[\w.$]+)\s+\d+\s*$',
                                  assembly, re.M)) - public)
    mapping = {name: name[0] + prefix + name[1:] for name in symbols}
    if not mapping:
        return assembly
    pattern = r'"(?:\\.|[^"\\])*"|(?<![\w.$])(?:' + '|'.join(
        re.escape(name) for name in sorted(mapping, key=len, reverse=True)) + r')(?![\w.$])'
    return re.sub(pattern, lambda m: mapping.get(m[0], m[0]), assembly)


def module_body(assembly):
    """Keep ordinary C's canonical function pointers, under our common header.

    Legacy headerless compiler modes deliberately use local call-stub
    addresses instead of export slots. Xt compares inherited method pointers,
    so use normal compilation and remove only its wrapper and import stubs.
    """
    assembly, count = re.subn(
        r'\A\s*align\s*\n\s*module[^\n]*\n\.ModStart:.*?^\s*init\s*\n',
        '', assembly, count=1, flags=re.M | re.S)
    if count != 1:
        raise ValueError('Expected an ordinary native Helios C module header')
    assembly = re.sub(r'^-- stubs\n.*?(?=^-- )', '', assembly,
                      count=1, flags=re.M | re.S)
    assembly, count = re.subn(r'^\s*data\s+\.MaxData\s+0\s*\n\s*align\s*\n'
                             r'\.ModEnd:\s*\Z', '', assembly, count=1, flags=re.M)
    if count != 1:
        raise ValueError('Expected an ordinary native Helios C module trailer')
    return assembly


def exports(objects):
    """Read compiler assembly, distinguishing function slots from C objects."""
    functions = set()
    data = {}
    for obj in objects:
        assembly = obj.with_suffix('.s').read_text()
        defined = set(re.findall(r'^([.][\w.$]+):', assembly, re.M))
        public = set(re.findall(r'^\s*global\s+(_[\w.$]+)\s*$', assembly, re.M))
        sizes = dict((n, int(size)) for n, size in re.findall(
            r'^\s*data\s+(_[\w.$]+)\s+(\d+)\s*$', assembly, re.M))
        for symbol in public:
            name = symbol[1:]
            if '.' + name in defined:
                functions.add(name)
            else:
                if symbol not in sizes:
                    raise ValueError('Missing size for exported data: ' + symbol)
                if name in data and data[name] != sizes[symbol]:
                    raise ValueError('Conflicting exported data: ' + name)
                data[name] = sizes[symbol]
    return sorted(functions), dict(sorted(data.items()))


def build(builder, target, context, objects):
    expand = lambda x: builder.expand(x, context)
    interface = json.loads(Path(expand(target['interface'])).read_text())
    actual_functions, actual_data = exports(objects)
    functions = interface['functions']
    public_data = interface['data']
    if set(functions) != set(actual_functions) or public_data != actual_data:
        raise ValueError('Resident interface differs from compiled exports: ' +
                         target['interface'])
    if len(functions) != len(set(functions)):
        raise ValueError('Duplicate resident function slots')
    work = Path(context['work'])
    folder = work / 'shared' / target['name']
    folder.mkdir(parents=True, exist_ok=True)
    resident = []
    defined = set()
    references = set()
    for obj in objects:
        assembly = module_body(obj.with_suffix('.s').read_text())
        own_functions, _ = exports([obj])
        assembly = private_symbols(assembly, own_functions, 'private_' + obj.stem[:12] + '_')
        own_exports = set(re.findall(r'^\s*global\s+(_[\w.$]+)\s*$', assembly, re.M))
        # The wrapper owns public data, ensuring fixed offsets independent
        # of the compiler's ordering of private objects in each source file.
        assembly = re.sub(r'^\s*data\s+(_[\w.$]+)\s+\d+\s*$',
            lambda m: '' if m[1] in own_exports and
                (m[1][1:] in public_data or m[1][1:] in functions) else m[0], assembly,
            flags=re.M)
        fixed = folder / obj.with_suffix('.s').name
        fixed.write_text(assembly)
        linked = fixed.with_suffix('.o')
        builder.run([builder.assembler, '-p', '-o', linked, fixed],
                    fixed.with_suffix('.asm.log'))
        resident.append(linked)
        defined.update(re.findall(r'^([.][\w.$]+):', assembly, re.M))
        references.update(re.findall(
            r'\b(?:call|j)\s+([.][A-Za-z_$][\w.$]*)', assembly))
        references.update(re.findall(
            r'\bldc\s+([.][A-Za-z_$][\w.$]*)-2', assembly))
    name = interface['name']
    declarations = ['extern func ' + f for f in functions]
    capacity = interface['function_capacity']
    if capacity < len(functions):
        raise ValueError('Resident function slots exhausted; define a new ABI')
    if capacity > len(functions):
        declarations.append('vec %d reserved_functions' % ((capacity-len(functions))*4))
    declarations += ['extern vec %d %s' % (words * 4, symbol)
                     for symbol, words in public_data.items()]
    wrapper = folder / 'start.a'
    body = ('include basic.m\ninclude library.m\nResident [\n' +
        ' name %s\n slot %d\n version %d\n static [\n  %s\n ]\n]\n' %
        (name, interface['slot'], interface['version'], '\n  '.join(declarations)))
    wrapper.write_text(body)
    macro = Path(context['sdk']) / 'include/ampp'
    ampp = Path(context['asm_dir']) / 'ampp'
    def assemble_wrapper(label, extra, source):
        output = folder / (label + '.s')
        builder.run([ampp, '-dhelios.TRAN', '1', *extra, '-i' + str(macro) + '/',
                     '-o' + str(output), macro / 'basic.m', source],
                    folder / (label + '.ampp.log'))
        builder.run([builder.assembler, '-p', '-o', output.with_suffix('.o'), output],
                    folder / (label + '.asm.log'))
    assemble_wrapper('start', [], wrapper)
    tail = folder / 'end.s'
    stubs = []
    for symbol in sorted(references - defined):
        slot = '_' + symbol[1:]
        stubs.append('%s:\n ldl 1\n ldnl 0\n ldnl @%s\n ldnl %s\n gcall\n' %
                     (symbol, slot, slot))
    tail.write_text(''.join(stubs) + ' data .MaxData 0\n align\n.ModEnd:\n')
    builder.run([builder.assembler, '-p', '-o', tail.with_suffix('.o'), tail],
                folder / 'end.asm.log')
    runtime = Path(expand(target['runtime']))
    runtime.parent.mkdir(parents=True, exist_ok=True)
    libraries = builder.link_libraries(builder.expand(target['libraries'], context))
    builder.run([builder.assembler, '-f', '-n' + name, '-o', runtime,
                 folder / 'start.o', *resident, folder / 'end.o',
                 *['-l' + lib for lib in libraries]], folder / 'runtime.link.log')
    # TaskInit follows each reference to the first module of a library. It
    # does not allocate/init additional ordinary modules hidden in that file.
    # Require one resident module and expose all dependencies to the client.
    image = runtime.read_bytes()
    offset = 12
    modules = []
    dependencies = []
    while offset + 48 <= len(image):
        kind, size = struct.unpack_from('<II', image, offset)
        if not kind:
            break
        if kind not in (0x60f160f1, 0x60f260f2) or size < 48 or offset+size > len(image):
            raise ValueError('Invalid resident image: ' + str(runtime))
        module_name = image[offset+8:offset+40].split(b'\0')[0].decode('ascii')
        (modules if kind == 0x60f160f1 else dependencies).append(module_name)
        offset += size
    if modules != [name]:
        raise ValueError('Resident library contains embedded ordinary modules: ' +
                         ', '.join(modules) + '; use resident dependencies')
    definition = folder / 'interface.a'
    definition.write_text(body[:-2] + ''.join(' uses '+n+'\n' for n in dependencies) + ']\n')
    assemble_wrapper('interface', ['-dmake.def', '1'], definition)
    output = Path(expand(target['output']))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes((folder/'interface.o').read_bytes())
    required=[]
    for library in libraries:
        path=Path(library+'.requires.json')
        if path.is_file() and json.loads(path.read_text())['name'] in dependencies:
            required.append(library)
    metadata=Path(str(output)+'.requires.json')
    metadata.write_text(json.dumps({'format':1,'name':name,'dependencies':required},indent=2)+'\n')
    outputs = [output, runtime, folder/'interface.o', metadata]
    for name in target.get('aliases', []):
        alias = Path(expand(name))
        alias.parent.mkdir(parents=True, exist_ok=True)
        alias.symlink_to(output)
        manifest=Path(str(alias)+'.requires.json')
        manifest.write_bytes(metadata.read_bytes())
        outputs.extend([alias,manifest])
    return outputs
