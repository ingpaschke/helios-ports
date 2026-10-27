#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Fetch, prepare, build and verify native Helios ports."""
from pathlib import Path
import argparse
import json
import sys

ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'tools'))
import sources
import release
from build import Builder


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=['list','show','prepare','refresh','build','verify','diff','package'])
    parser.add_argument('ports',nargs='*')
    parser.add_argument('--work',type=Path,default=ROOT/'work')
    parser.add_argument('--sdk',type=Path)
    parser.add_argument('--asm',type=Path)
    parser.add_argument('--compat',type=Path)
    parser.add_argument('--toolchain',type=Path,default=Path('/opt/llvm-t800'))
    parser.add_argument('--cpu', choices=['generic', 't425', 't800'])
    parser.add_argument('--linkage', choices=['static', 'shared'])
    parser.add_argument('--profile', help='release profile from profiles/')
    parser.add_argument('--output', type=Path, help='runtime archive for package')
    parser.add_argument('--release', default='unreleased', help='runtime archive release identifier')
    parser.add_argument('--host-sdk',type=Path)
    parser.add_argument('--jobs',type=int,default=4)
    parser.add_argument('--offline',action='store_true')
    parser.add_argument('--force',action='store_true')
    parser.add_argument('--edited',type=Path,help='clean edited source tree for diff')
    parser.add_argument('--patch',help='append this patch, or update it if it is last; default: update the last patch')
    parser.add_argument('--description',help='one-sentence purpose of the patch')
    args=parser.parse_args()
    profile=release.profile(ROOT,args.profile) if args.profile else None
    if profile:
        for key in ('cpu','linkage'):
            if getattr(args,key) and getattr(args,key)!=profile[key]:
                parser.error('--'+key+' conflicts with the selected profile')
            setattr(args,key,profile[key])
    args.cpu=args.cpu or 't800'; args.linkage=args.linkage or 'shared'
    names=sources.names()
    if args.command=='list':
        for name in names:print(name,sources.configuration(name)['version'])
        return
    selected=args.ports or (profile['ports'] if profile else names)
    for name in selected:
        if name not in names:parser.error('Unknown port: '+name)
    if args.command=='show':
        for name in selected:
            config=sources.configuration(name)
            print(name+' '+config['version'])
            print('  Upstream: '+config['upstream']['url'])
            print('  Dependencies: '+(', '.join(config['dependencies']) or 'none'))
            for patch in config['patches']:
                print('  '+patch['file']+': '+patch.get('description','Helios adaptation'))
            if config.get('files'): print('  New files: ports/'+name+'/'+config['files']+'/')
        return
    if args.command=='refresh':
        for name in selected:
            sources.refresh(name,args.work,offline=args.offline)
            print(name+': source checksums refreshed')
        return
    if args.command=='prepare':
        for name in selected:print(sources.prepare(name,args.work,offline=args.offline))
        return
    if args.command=='diff':
        if len(selected)!=1 or not args.edited:parser.error('diff requires one port and --edited')
        sources.edit_patch(selected[0],args.edited,args.work,patch_name=args.patch,
                           description=args.description,offline=args.offline)
        return
    if not args.sdk or not args.asm:parser.error('build/verify require --sdk and --asm')
    if args.jobs<1:parser.error('--jobs must be positive')
    builder=Builder(args.work,args.sdk,args.asm,args.compat,args.toolchain,args.jobs,args.host_sdk,args.offline,cpu=args.cpu,linkage=args.linkage)
    if args.command=='package':
        if not profile or not args.output:parser.error('package requires --profile and --output')
        release.archive(builder,selected,profile,args.output,args.release)
        print(args.output);return
    for name in selected:
        if args.command=='build':builder.build(name,args.force)
        else:builder.validate(name);print(name+': verified')


if __name__=='__main__':
    try:main()
    except (OSError,ValueError,RuntimeError) as error:sys.exit(str(error))
