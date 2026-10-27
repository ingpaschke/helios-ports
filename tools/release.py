# SPDX-License-Identifier: MIT
"""Collect verified port stages into portable Helios runtime archives."""
import hashlib
import gzip
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import tarfile
import tempfile
import zipfile

SHORT = re.compile(r"[A-Za-z0-9_$~!#%&{}()@'`^-]{1,8}(\.[A-Za-z0-9_$~!#%&{}()@'`^-]{1,3})?\Z")


def profile(root, name):
    if not re.fullmatch(r'[a-z0-9-]+', name): raise ValueError('Invalid profile name')
    value = json.loads((Path(root) / 'profiles' / (name + '.json')).read_text())
    if value['name'] != name or value['format'] != 1: raise ValueError('Invalid release profile')
    return value


def records(data):
    """Read native image records, including resident IDs and ABI versions."""
    if data[:4] != b'\x78\x56\x34\x12': return []
    out = []; offset = 12
    while offset + 8 <= len(data):
        kind, size = struct.unpack_from('<II', data, offset)
        if not kind: break
        if kind not in (0x60f060f0, 0x60f160f1, 0x60f260f2) or size < 48 or offset + size > len(data):
            raise ValueError('Invalid native module record')
        name = data[offset+8:offset+40].split(b'\0')[0].decode('ascii')
        slot, version = struct.unpack_from('<II', data, offset+40)
        out.append({'name': name, 'slot': slot, 'version': version,
                    'reference': kind == 0x60f260f2, 'resident': kind == 0x60f160f1})
        offset += size
    return out


def runtime_path(path):
    """SDK archives, headers, source bundles and regression programs stay out."""
    parts = PurePosixPath(path).parts
    if any(p in ('include', 'x11r4', 'tests') for p in parts): return False
    if 'libexec' in parts and ('test' in parts[-1] or 'probe' in parts[-1]): return False
    if parts[-1].endswith(('.a', '.lib', '.def', '.requires.json')): return False
    if path.startswith('lib/') and path.endswith('.json'): return False
    if parts[-1] == 'source.tar.gz': return False
    return True


def mapped(path, settings):
    aliases = settings.get('aliases', {})
    if path in aliases: return aliases[path]
    if any(path == root or path.startswith(root + '/') for root in settings.get('shorten_assets', [])):
        root = next(root for root in settings['shorten_assets']
                    if path == root or path.startswith(root + '/'))
        tail = PurePosixPath(path).relative_to(root)
        def short(part):
            stem, dot, extension = part.partition('.')
            return stem[:8] + (dot + extension[:3] if dot else '')
        return str(PurePosixPath(root, *(short(p) for p in tail.parts)))
    return path


def closure(builder, selected):
    found = []
    def visit(name):
        if name in found: return
        for dependency in builder.config(name)['dependencies']: visit(dependency)
        found.append(name)
    for name in selected: visit(name)
    return found


def collect(builder, selected, settings):
    """Return canonical source files and a manifest; never change installed data."""
    if settings['cpu'] != builder.cpu or settings['linkage'] != builder.linkage:
        raise ValueError('Release CPU/linkage does not match the builder')
    files = {}; folded = {}; builds = {}
    for name in closure(builder, selected):
        info = builder.validate(name); builds[name] = info
        stage = builder.work / name / 'stage'
        for source in sorted(stage.rglob('*')):
            if not source.is_file(): continue
            canonical = source.relative_to(stage).as_posix()
            if not runtime_path(canonical): continue
            if builder.linkage == 'static' and canonical == 'lib/Hxb': continue
            if any(PurePosixPath(canonical).match(p) for p in settings.get('exclude', [])): continue
            if canonical.startswith('local/share/terminfo/') and settings.get('terminfo'):
                if source.name not in settings['terminfo']: continue
            destination = mapped(canonical, settings)
            if destination is None: continue
            parts = PurePosixPath(destination).parts
            if not parts or destination.startswith('/') or '..' in parts:
                raise ValueError('Invalid runtime path: ' + destination)
            if settings.get('short_names') and not all(SHORT.fullmatch(p) for p in parts):
                raise ValueError('Not 8.3: ' + destination)
            digest = hashlib.sha256(source.read_bytes()).hexdigest()
            # Materialize aliases; no host build symlinks enter a runtime archive.
            key = destination.casefold()
            if key in folded and folded[key] != destination:
                raise ValueError('Case collision: ' + destination)
            folded[key] = destination
            if destination in files:
                if files[destination]['sha256'] != digest:
                    raise ValueError('Conflicting runtime files: ' + destination)
                continue
            files[destination] = {'source': source, 'port': name, 'sha256': digest,
                                  'canonical': canonical,
                                  'executable': bool(source.stat().st_mode & 0o111)}
    # Explicit profile defaults replace resources, never application code.
    for destination, relative in settings.get('resources', {}).items():
        if destination not in files or not destination.startswith('lib/x11/app-defa/'):
            raise ValueError('Resource override is not an installed application default: ' + destination)
        folder = builder.root / 'profiles'
        source = (folder / relative).resolve()
        if not source.is_relative_to(folder.resolve()) or not source.is_file():
            raise ValueError('Invalid profile resource: ' + relative)
        files[destination] = dict(files[destination], source=source,
            sha256=hashlib.sha256(source.read_bytes()).hexdigest(), executable=False,
            profile_resource=relative)
    provided = {}; required = {}
    for path, item in files.items():
        for record in records(item['source'].read_bytes()):
            if (record['resident'] and PurePosixPath(path).parent == PurePosixPath('lib')
                    and record['name'].casefold() == PurePosixPath(path).name.casefold()):
                provided[record['name'].casefold()] = dict(record, path=path)
            elif record['reference']:
                old = required.get(record['name'].casefold())
                if old and (old['slot'] != record['slot'] or old['version'] != record['version']):
                    raise ValueError('Mixed library interfaces: ' + record['name'])
                required[record['name'].casefold()] = record
    external = []
    resident_names = set()
    for name in closure(builder, selected):
        for target in builder.config(name)['build'].get('targets', []):
            if target['type'] == 'shared': resident_names.add(target['name'].casefold())
    for key, reference in required.items():
        actual = provided.get(key)
        if actual:
            if actual['slot'] != reference['slot'] or actual['version'] != reference['version']:
                raise ValueError('Incompatible resident library: ' + reference['name'])
        elif key in resident_names:
            raise ValueError('Missing resident library: ' + reference['name'])
        else: external.append({k: reference[k] for k in ['name', 'slot', 'version']})
    manifest = {'format': 1, 'profile': settings['name'], 'cpu': builder.cpu,
                'linkage': builder.linkage, 'helios': '1.31-SP2', 'selected': selected,
                'ports': {name: {'version': info['version'], 'inputs': info['inputs'],
                                 'upstream': builder.config(name)['upstream']}
                          for name, info in builds.items()},
                'libraries': list(provided.values()), 'core_libraries': external,
                'files': {path: {k: value for k, value in item.items() if k != 'source'}
                          for path, item in files.items()}}
    return files, manifest


def install(builder, selected, settings, destination):
    """Merge the verified runtime into a distribution's /helios tree."""
    files, manifest = collect(builder, selected, settings)
    destination = Path(destination)
    for path, item in files.items():
        target = destination / path
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.is_symlink(): target.unlink()
        shutil.copy2(item['source'], target)
    return manifest


def archive(builder, selected, settings, output, version):
    files, manifest = collect(builder, selected, settings)
    manifest['release'] = version
    output = Path(output).resolve(); output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='helios-ports-') as folder:
        root = Path(folder); (root / 'helios').mkdir()
        for name, item in files.items():
            target = root / 'helios' / name; target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(item['source'], target)
        (root / 'ports.jsn').write_text(json.dumps(manifest, indent=2) + '\n')
        (root / 'readme.txt').write_text(
            f'Helios ports {version}: {settings["name"]}\n'
            f'CPU: {builder.cpu}; X libraries: {builder.linkage}; requires Helios 1.31-SP2.\n'
            'Copy the contents of helios/ into the target /helios tree.\n'
            'On Atari, use the physical Helios folder selected by your launcher.\n'
            'Keep shared libraries and their clients from the same release together.\n'
            'This archive contains applications and libraries, not a bootable system.\n'
            'Desktop startup and user configuration are supplied by the distribution.\n')
        paths = [p for p in sorted(root.rglob('*')) if p.is_file()]
        (root / 'sha256.txt').write_text(''.join(
            hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.relative_to(root).as_posix() + '\n'
            for p in paths))
        if settings.get('short_names'):
            with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED) as out:
                for p in sorted(root.rglob('*')):
                    if not p.is_file(): continue
                    entry = zipfile.ZipInfo(p.relative_to(root).as_posix(), (2000, 1, 1, 0, 0, 0))
                    entry.compress_type = zipfile.ZIP_DEFLATED
                    entry.external_attr = (0o100755 if p.stat().st_mode & 0o111 else 0o100644) << 16
                    out.writestr(entry, p.read_bytes())
        else:
            def canonical(info):
                info.uid = info.gid = info.mtime = 0
                info.uname = info.gname = ''
                return info
            with output.open('wb') as destination, gzip.GzipFile(fileobj=destination, mode='wb', mtime=0, filename='') as compressed:
                with tarfile.open(fileobj=compressed, mode='w') as out:
                    for p in sorted(root.iterdir()): out.add(p, arcname=p.name, filter=canonical)
    output.with_suffix(output.suffix + '.sha256').write_text(
        hashlib.sha256(output.read_bytes()).hexdigest() + '  ' + output.name + '\n')
    return manifest
