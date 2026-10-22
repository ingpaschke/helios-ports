# SPDX-License-Identifier: MIT
"""Pinned upstream sources and exact Helios patches; no vendored source trees."""
from pathlib import Path
import fnmatch
import hashlib
import json
import os
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def inventory(root):
    result = {}
    for path in sorted(Path(root).rglob('*')):
        if '.git' in path.relative_to(root).parts or '__pycache__' in path.parts:
            continue
        name = path.relative_to(root).as_posix()
        if path.is_symlink():
            result[name] = {'link': os.readlink(path)}
        elif path.is_file():
            result[name] = {'sha256': sha(path), 'executable': bool(path.stat().st_mode & 0o111)}
    return result


def tree_hash(root):
    return hashlib.sha256(json.dumps(inventory(root), sort_keys=True).encode()).hexdigest()


def configuration(name, root=ROOT):
    path = Path(root)/'ports'/name/'port.json'
    config = json.loads(path.read_text())
    if config.get('format') != 1 or config.get('name') != name:
        raise ValueError('Invalid port configuration: ' + str(path))
    return config


def names(root=ROOT):
    return sorted(p.parent.name for p in (Path(root)/'ports').glob('*/port.json'))


def download(url, target):
    target = Path(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_suffix(target.suffix + '.part')
    try:
        request = urllib.request.Request(url, headers={'User-Agent': 'helios-ports/1'})
        with urllib.request.urlopen(request, timeout=60) as response, temporary.open('wb') as out:
            shutil.copyfileobj(response, out)
        temporary.replace(target)
    finally:
        temporary.unlink(missing_ok=True)


def extract(archive, destination, spec):
    prefix = spec.get('subdirectory', '').strip('/')
    with tarfile.open(archive) as bundle:
        members = []
        for member in bundle.getmembers():
            name = member.name.removeprefix('./').rstrip('/')
            if prefix:
                if not name.startswith(prefix + '/'): continue
                relative = name[len(prefix)+1:]
            else:
                relative = name
            if not member.isdir() and not any(fnmatch.fnmatchcase(relative, p) for p in spec.get('include', ['**'])):
                continue
            members.append(member)
        bundle.extractall(destination, members=members, filter='data')


def upstream(config, destination, cache, offline=False):
    """Extract the pinned source, selecting its configured subtree/files."""
    spec = config['upstream']; cache = Path(cache); cache.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='upstream-', dir=cache) as temporary:
        scratch = Path(temporary)
        if spec['kind'] == 'archive':
            archive = cache/spec['filename']
            if not archive.exists():
                if offline: raise RuntimeError('Missing cached archive: ' + str(archive))
                download(spec['url'], archive)
            if sha(archive) != spec['sha256']:
                raise RuntimeError('Upstream checksum mismatch: ' + str(archive))
        elif spec['kind'] == 'git':
            repository = cache/(config['name'] + '.git')
            if not repository.exists():
                if offline: raise RuntimeError('Missing cached repository: ' + str(repository))
                subprocess.run(['git', 'init', '--bare', str(repository)], check=True, capture_output=True)
            found = subprocess.run(['git', '--git-dir='+str(repository), 'cat-file', '-e', spec['revision']+'^{commit}'], capture_output=True)
            if found.returncode:
                if offline: raise RuntimeError('Missing pinned revision: ' + spec['revision'])
                subprocess.run(['git', '--git-dir='+str(repository), 'fetch', '--depth=1', spec['url'], spec['revision']], check=True)
            archive = scratch/'source.tar'
            with archive.open('wb') as out:
                subprocess.run(['git', '--git-dir='+str(repository), 'archive', spec['revision']], stdout=out, check=True)
        else:
            raise ValueError('Unknown upstream kind: ' + spec['kind'])
        extracted = scratch/'extracted'; extracted.mkdir()
        extract(archive, extracted, spec)
        source = extracted/spec.get('subdirectory', '')
        if not source.is_dir(): raise RuntimeError('Missing upstream subdirectory: ' + str(source))
        destination = Path(destination); destination.mkdir(parents=True, exist_ok=False)
        patterns = spec.get('include', ['**'])
        for path in sorted(source.rglob('*')):
            relative = path.relative_to(source)
            if not any(fnmatch.fnmatchcase(relative.as_posix(), pattern) for pattern in patterns): continue
            target = destination/relative
            if path.is_symlink():
                target.parent.mkdir(parents=True, exist_ok=True); target.symlink_to(os.readlink(path))
            elif path.is_file():
                target.parent.mkdir(parents=True, exist_ok=True); shutil.copy2(path, target)
        if tree_hash(destination) != spec['tree_sha256']:
            raise RuntimeError('Upstream tree mismatch: ' + config['name'])


def identity(config, root=ROOT):
    parts = [json.dumps(config, sort_keys=True), sha(Path(__file__))]
    for patch in config['patches']:
        path = Path(root)/'ports'/config['name']/patch['file']
        if sha(path) != patch['sha256']: raise RuntimeError('Patch checksum mismatch: ' + str(path))
        parts.append(patch['sha256'])
    if config.get('files'):
        overlay = Path(root)/'ports'/config['name']/config['files']
        if not overlay.is_dir(): raise RuntimeError('Missing port files: '+str(overlay))
        parts.append(tree_hash(overlay))
    return hashlib.sha256('\0'.join(parts).encode()).hexdigest()


def copy_paths(source, destination, paths):
    """Copy source files without following symlinks or losing executable bits."""
    for relative in paths:
        src, dst = Path(source)/relative, Path(destination)/relative
        dst.parent.mkdir(parents=True, exist_ok=True)
        if src.is_symlink(): dst.symlink_to(os.readlink(src))
        else: shutil.copy2(src, dst)


def apply_patches(config, tree, root, patches=None):
    patches = config['patches'] if patches is None else patches
    subprocess.run(['git', 'init', '-q', str(tree)], check=True)
    subprocess.run(['git', '-C', str(tree), 'add', '-f', '.'], check=True)
    for patch in patches:
        path = (Path(root)/'ports'/config['name']/patch['file']).resolve()
        subprocess.run(['git', '-C', str(tree), 'apply', '--index',
                        '--whitespace=nowarn', str(path)], check=True)


def add_files(config, tree, root, upstream_paths):
    if not config.get('files'): return
    overlay = Path(root)/'ports'/config['name']/config['files']
    if not overlay.is_dir(): raise RuntimeError('Missing port files: '+str(overlay))
    paths = inventory(overlay)
    for relative in paths:
        target = tree/relative
        if relative in upstream_paths or target.exists() or target.is_symlink():
            raise RuntimeError('Port files must not replace upstream or patched files: '+relative)
    copy_paths(overlay, tree, paths)


def save_configuration(config, root):
    path = Path(root)/'ports'/config['name']/'port.json'
    path.write_text(json.dumps(config, indent=2)+'\n')


def refresh(name, work, root=ROOT, offline=False):
    """Record intentional edits to patches/files while keeping upstream pinned."""
    config = configuration(name, root)
    for patch in config['patches']:
        patch['sha256'] = sha(Path(root)/'ports'/name/patch['file'])
    with tempfile.TemporaryDirectory(prefix='port-refresh-') as temporary:
        tree = Path(temporary)/'tree'
        upstream(config, tree, Path(work)/'cache', offline)
        upstream_paths = inventory(tree)
        apply_patches(config, tree, root)
        add_files(config, tree, root, upstream_paths)
        config['patched_tree_sha256'] = tree_hash(tree)
    save_configuration(config, root)


def prepare(name, work, root=ROOT, offline=False):
    config = configuration(name, root); key = identity(config, root)
    work = Path(work).resolve(); destination = work/'sources'/name
    record = work/'sources'/(name+'.json')
    if destination.is_dir() and record.is_file():
        info = json.loads(record.read_text())
        if info.get('identity') == key and tree_hash(destination) == config['patched_tree_sha256']:
            return destination
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=name+'-', dir=destination.parent) as temporary:
        tree = Path(temporary)/'tree'
        upstream(config, tree, work/'cache', offline)
        upstream_paths = inventory(tree)
        apply_patches(config, tree, root)
        add_files(config, tree, root, upstream_paths)
        shutil.rmtree(tree/'.git')
        if tree_hash(tree) != config['patched_tree_sha256']:
            raise RuntimeError('Source tree changed: '+name+'; review edits, then run refresh '+name)
        if destination.exists(): shutil.rmtree(destination)
        tree.replace(destination)
    record.write_text(json.dumps({'identity': key, 'tree_sha256': config['patched_tree_sha256']}, indent=2)+'\n')
    return destination


def edit_patch(name, edited, work, root=ROOT, patch_name=None, description=None, offline=False):
    """Update/append an upstream patch; store additions as ordinary port files."""
    config = configuration(name, root)
    identity(config, root)
    directory = Path(root)/'ports'/name
    patches = config['patches']
    patch_name = patch_name or (patches[-1]['file'] if patches else 'helios.patch')
    if Path(patch_name).name != patch_name or not patch_name.endswith('.patch'):
        raise ValueError('Use a plain .patch filename')
    previous = next((p for p in patches if p['file'] == patch_name), None)
    if previous is not None and previous is not patches[-1]:
        raise ValueError('Only the last patch can be updated; append a new patch for a further change')
    preceding = patches[:-1] if previous else patches[:]
    description = description if description is not None else (previous or {}).get('description')
    with tempfile.TemporaryDirectory(prefix='port-diff-') as temporary:
        base = Path(temporary)/'base'
        upstream(config, base, Path(work)/'cache', offline)
        upstream_paths = inventory(base)
        apply_patches(config, base, root, preceding)
        original = subprocess.check_output(['git', '-C', str(base), 'write-tree'], text=True).strip()
        for path in base.iterdir():
            if path.name == '.git': continue
            if path.is_dir() and not path.is_symlink(): shutil.rmtree(path)
            else: path.unlink()
        edited_paths = inventory(edited)
        copy_paths(edited, base, [p for p in edited_paths if p in upstream_paths])
        subprocess.run(['git', '-C', str(base), 'add', '-A', '-f'], check=True)
        patch = directory/patch_name
        delta = subprocess.check_output(['git', '-C', str(base), 'diff', '--cached', '--binary', '--full-index', original])
        config['patches'] = preceding
        if delta:
            heading = ('Subject: '+description+'\n\n').encode() if description else b''
            patch.write_bytes(heading+delta)
            entry = {'file': patch.name, 'sha256': sha(patch)}
            if description: entry['description'] = description
            config['patches'].append(entry)
        else:
            patch.unlink(missing_ok=True)
        additions = [p for p in edited_paths if p not in upstream_paths]
        overlay = directory/config.get('files', 'files')
        if overlay.exists(): shutil.rmtree(overlay)
        if additions:
            config['files'] = overlay.relative_to(directory).as_posix()
            copy_paths(edited, overlay, additions)
            copy_paths(edited, base, additions)
        else:
            config.pop('files', None)
        config['patched_tree_sha256'] = tree_hash(base)
    save_configuration(config, root)
