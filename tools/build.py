# SPDX-License-Identifier: MIT
"""Shared compilation, linking, staging and verification for configured ports."""
from pathlib import Path
import concurrent.futures
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import sources


class Builder:
    def __init__(self, work, sdk, assembler, compat=None, toolchain='/opt/llvm-t800',
                 jobs=4, host_sdk=None, offline=False, root=sources.ROOT, cpu='t800'):
        self.root = Path(root).resolve(); self.work = Path(work).resolve()
        self.sdk = Path(sdk).resolve(); self.assembler = Path(assembler).resolve()
        self.compat = Path(compat).resolve() if compat else None
        self.toolchain = Path(toolchain).resolve(); self.jobs = jobs
        self.host_sdk = Path(host_sdk).resolve() if host_sdk else None
        if cpu not in ('generic', 't425', 't800'):
            raise ValueError('Unsupported CPU: ' + cpu)
        self.cpu = cpu
        self.offline = offline; self.completed = {}; self.active = set()
        self.work.mkdir(parents=True, exist_ok=True)

    def config(self, name):
        return sources.configuration(name, self.root)

    def context(self, name):
        config = self.config(name)
        src = sources.prepare(name, self.work, self.root, self.offline)
        context = {'src': str(self.work/name/'src'), 'original': str(src),
                   'work': str(self.work/name), 'stage': str(self.work/name/'stage'),
                   'sdk': str(self.sdk), 'asm': str(self.assembler), 'asm_dir': str(self.assembler.parent),
                   'toolchain': str(self.toolchain), 'tools': str(self.toolchain/'sysroots/helios'),
                   'clang': str(self.toolchain/'bin/clang'), 'hcc': str(self.toolchain/'bin/hcc'),
                   'runtime': str(self.toolchain/'sysroots/helios/lib/llvmrt-generic.lib'),
                   'python': sys.executable, 'jobs': str(self.jobs),
                   'compat': str(self.compat or ''), 'host_sdk': str(self.host_sdk or '')}
        for dependency in config['dependencies']:
            context['dep:'+dependency] = str(self.work/dependency)
            context['src:'+dependency] = str(self.work/dependency/'src')
        return context

    @staticmethod
    def expand(value, context):
        if isinstance(value, list): return [Builder.expand(x, context) for x in value]
        return re.sub(r'\$\{([^}]+)\}', lambda m: context[m[1]], str(value))

    @staticmethod
    def run(command, log, cwd=None, stdout=None):
        log = Path(log); log.parent.mkdir(parents=True, exist_ok=True)
        command = list(map(str, command))
        with log.open('w') as out:
            out.write(json.dumps(command)+'\n'); out.flush()
            if stdout is None:
                result = subprocess.run(command, cwd=cwd, stdout=out, stderr=subprocess.STDOUT)
            else:
                destination = Path(stdout)
                destination.parent.mkdir(parents=True, exist_ok=True)
                with destination.open('wb') as generated:
                    result = subprocess.run(command, cwd=cwd, stdout=generated, stderr=out)
        text = log.read_text(errors='replace')
        # Clang prints numbered source excerpts beneath warnings. A string
        # literal such as "Notifier fatal error" is not a build diagnostic.
        diagnostics = '\n'.join(line for line in text.splitlines()[1:]
                                if not re.match(r'\s*(?:\d+\s*\||\|)', line))
        if result.returncode or re.search(r'undefined reference|undefined symbol|Fatal Error', diagnostics, re.I):
            raise RuntimeError(str(log)+'\n'+'\n'.join(text.splitlines()[-15:]))

    def fingerprint(self, name):
        config = self.config(name)
        result = {'configuration': sources.identity(config, self.root),
                  'cpu': self.cpu,
                  'engine': sources.tree_hash(self.root/'tools'),
                  'entry_point': sources.sha(self.root/'ports.py'),
                  'sdk': sources.tree_hash(self.sdk/'include')+sources.tree_hash(self.sdk/'lib'),
                  'assembler': sources.sha(self.assembler),
                  'compiler': sources.sha(self.toolchain/'bin/clang'),
                  'hcc': sources.sha(self.toolchain/'bin/hcc'),
                  'runtime': sources.sha(self.toolchain/'sysroots/helios/lib/llvmrt-generic.lib')}
        if config.get('requires_compat'):
            if not self.compat or not self.compat.is_dir(): raise RuntimeError(name+' requires an existing --compat stage')
            result['compat'] = sources.tree_hash(self.compat)
        if config.get('requires_host_sdk'):
            if not self.host_sdk or not self.host_sdk.is_dir(): raise RuntimeError(name+' requires --host-sdk for ILP32 data generators')
            result['host_sdk'] = sources.tree_hash(self.host_sdk)
        if config.get('cxx'):
            result['cxx'] = sources.tree_hash(self.toolchain/'sysroots/helios/include/c++')
            for file in ('cxx0.o','llvmcxx-generic.lib'):
                result[file] = sources.sha(self.toolchain/'sysroots/helios/lib'/file)
        for dependency in config['dependencies']:
            result['dependency:'+dependency] = sources.sha(self.work/dependency/'build-info.json')
        for program in config.get('host_tools', []):
            path = shutil.which(program)
            if not path: raise RuntimeError('Missing host tool: '+program)
            result['host-tool:'+program] = sources.sha(path)
        for path in config.get('build_inputs', []):
            path = Path(self.expand(path,self.context(name)))
            result['build-input:'+str(path)] = sources.sha(path)
        return result

    def validate(self, name, active=None):
        active=set() if active is None else active
        if name in active:raise RuntimeError('Dependency cycle: '+name)
        active.add(name)
        for dependency in self.config(name)['dependencies']:self.validate(dependency,active)
        active.remove(name)
        path = self.work/name/'build-info.json'
        info = json.loads(path.read_text())
        if info['inputs'] != self.fingerprint(name): raise RuntimeError('Rebuild stale port: '+name)
        for relative, digest in info['outputs'].items():
            output = self.work/name/relative
            actual = {'link': os.readlink(output)} if output.is_symlink() else {
                'sha256': sources.sha(output), 'executable': bool(output.stat().st_mode & 0o111)}
            if actual != digest: raise RuntimeError('Modified port output: '+str(output))
        if info['stage_files'] != sources.inventory(self.work/name/'stage'):
            raise RuntimeError('Modified installation stage: '+name)
        return info

    def actions(self, actions, context, label):
        for number, action in enumerate(actions):
            kind = action['type']; expand = lambda x: self.expand(x, context)
            if kind == 'run':
                self.run(expand(action['command']), Path(context['work'])/'logs'/f'{label}-{number}.log',
                         cwd=expand(action.get('cwd','${src}')),
                         stdout=expand(action['stdout']) if 'stdout' in action else None)
            elif kind == 'copy':
                src = Path(expand(action['source'])); dest = Path(expand(action['destination']))
                dest.parent.mkdir(parents=True, exist_ok=True)
                if src.is_dir(): shutil.copytree(src, dest, dirs_exist_ok=True, symlinks=True)
                else: shutil.copy2(src, dest)
            elif kind in ('glob-copy', 'glob-link'):
                src = Path(expand(action['source'])); dest = Path(expand(action['destination']))
                dest.mkdir(parents=True, exist_ok=True)
                for path in sorted(src.glob(action['pattern'])):
                    target = dest/path.name
                    if target.is_symlink() or target.is_file(): target.unlink()
                    if kind == 'glob-link': target.symlink_to(path.resolve())
                    else: shutil.copy2(path, target)
            elif kind == 'symlink':
                dest = Path(expand(action['destination'])); dest.parent.mkdir(parents=True, exist_ok=True)
                if dest.is_symlink() or dest.is_file(): dest.unlink()
                dest.symlink_to(expand(action['source']))
            elif kind == 'text-copy':
                src = Path(expand(action['source'])); dest = Path(expand(action['destination']))
                pairs = [(src, dest)]
                if 'pattern' in action:
                    pairs = [(p, dest/p.name.removesuffix(action.get('strip_suffix', '')))
                             for p in sorted(src.glob(action['pattern'])) if p.is_file()]
                    if not pairs: raise RuntimeError('No text inputs matched: '+str(src/action['pattern']))
                for source, destination in pairs:
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    text = source.read_text()
                    for old,new in action.get('replace', {}).items(): text = text.replace(old,new)
                    destination.write_text(text)
            else: raise ValueError('Unknown build action: '+kind)

    def compile(self, name, group, entry, context, identity):
        if isinstance(entry, str): entry = {'file': entry}
        source = Path(self.expand(entry['file'], context))
        local = dict(context, source_dir=str(source.parent))
        label = hashlib.sha256(str(source).encode()).hexdigest()[:12]+'-'+source.stem
        out = Path(context['work'])/'obj'/group['name']/label
        out.parent.mkdir(parents=True, exist_ok=True)
        flags = self.expand(group.get('flags',[])+entry.get('flags',[]), local)
        if group.get('compiler','clang') == 'hcc':
            includes = self.expand(group['includes'], local)
            cpu_flag = {'generic': '-t4', 't425': '-t5', 't800': '-t8'}[self.cpu]
            command = [context['hcc'], *flags, cpu_flag, '-i,'+','.join(p+'/' for p in includes),
                       '-j'+','.join(p+'/' for p in includes), str(source), '-s'+str(out.with_suffix('.s'))]
        else:
            command = [context['clang'], '--target=t800-helios', '-mcpu='+self.cpu, '--sysroot='+context['sdk'],
                       '-Os', *flags, '-S', str(source), '-o', str(out.with_suffix('.s'))]
        stamp = out.with_suffix('.sha256')
        key = hashlib.sha256((identity+json.dumps(command)+sources.sha(source)).encode()).hexdigest()
        if not out.with_suffix('.o').is_file() or not stamp.is_file() or stamp.read_text()!=key:
            self.run(command, out.with_suffix('.compile.log'))
            self.run([self.assembler,'-p','-o',out.with_suffix('.o'),out.with_suffix('.s')],out.with_suffix('.asm.log'))
            stamp.write_text(key)
        return out.with_suffix('.o')

    def build(self, name, force=False):
        if name in self.completed: return self.completed[name]
        if name in self.active: raise ValueError('Dependency cycle: '+name)
        self.active.add(name)
        config = self.config(name)
        for dependency in config['dependencies']: self.build(dependency, force)
        context = self.context(name); work = Path(context['work']); work.mkdir(parents=True, exist_ok=True)
        inputs = self.fingerprint(name)
        if not force:
            try:
                info = self.validate(name)
                self.completed[name]=info;self.active.remove(name)
                print(name+': current',flush=True);return info
            except (OSError,ValueError,KeyError,RuntimeError): pass
        print(name+': building',flush=True)
        (work/'build-info.json').unlink(missing_ok=True)
        src = Path(context['src']); stage = Path(context['stage'])
        # Derived headers, symlinks and libraries must not survive a rebuild.
        # Keep only fingerprinted object caches and diagnostic logs.
        for path in work.iterdir():
            if path.name in ('obj','logs'): continue
            if path.is_dir() and not path.is_symlink(): shutil.rmtree(path)
            else: path.unlink()
        shutil.copytree(context['original'],src,symlinks=True);stage.mkdir()
        spec=config['build']; self.actions(spec.get('prepare',[]),context,'prepare')
        identity=hashlib.sha256(json.dumps(inputs,sort_keys=True).encode()).hexdigest()
        objects={}
        for group in spec.get('groups',[]):
            defaults = spec.get('compile', {})
            merged = dict(defaults, **group)
            for field in ('flags', 'includes'):
                merged[field] = defaults.get(field, []) + group.get(field, [])
            with concurrent.futures.ThreadPoolExecutor(max_workers=self.jobs) as pool:
                futures=[pool.submit(self.compile,name,merged,entry,context,identity) for entry in group['sources']]
                objects[group['name']]=[f.result() for f in futures]
        outputs=[]
        for target in spec.get('targets',[]):
            if target['type'] != 'archive': target = dict(spec.get('link', {}), **target)
            output=Path(self.expand(target['output'],context));output.parent.mkdir(parents=True,exist_ok=True)
            obj=[p for group in target.get('groups',[]) for p in objects[group]]
            if target['type']=='archive': command=[self.assembler,'-p','-o',output,*obj]
            else:
                command=[self.assembler,'-n'+target['name'],'-o',output]
                for flag,setting in (('-s','stack'),('-h','heap')):
                    if setting in target:command.append(flag+str(target[setting]))
                command += [self.expand(target.get('start','${sdk}/lib/c0.o'),context),*obj]
                command += self.expand(target.get('objects',[]),context)
                command += ['-l'+x for x in self.expand(target.get('libraries',[]),context)]
            self.run(command,work/'logs'/(target['name']+'.link.log'))
            if target['type']!='archive':output.chmod(0o755)
            outputs.append(output)
        self.actions(spec.get('install',[]),context,'install')
        paths=outputs+list(stage.rglob('*'))
        for folder in spec.get('outputs',[]):
            p=Path(self.expand(folder,context))
            if not p.exists() and not p.is_symlink(): raise RuntimeError('Missing declared output: '+str(p))
            paths += list(p.rglob('*')) if p.is_dir() else [p]
        records={}
        for path in sorted(set(paths)):
            if path.is_symlink():value={'link':os.readlink(path)}
            elif path.is_file():value={'sha256':sources.sha(path),'executable':bool(path.stat().st_mode & 0o111)}
            else:continue
            records[path.relative_to(work).as_posix()]=value
        info={'format':1,'name':name,'version':config['version'],'inputs':inputs,'outputs':records,
              'stage_files':sources.inventory(stage),'stage':'stage','config':'ports/'+name+'/port.json'}
        (work/'build-info.json').write_text(json.dumps(info,indent=2)+'\n')
        self.validate(name);self.completed[name]=info;self.active.remove(name)
        print(name+': PASS',flush=True)
        return info
