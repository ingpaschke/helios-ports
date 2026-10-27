#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Test the lossless cache and allocation failures with a native host compiler."""
import argparse,os,shlex,subprocess,tempfile
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--sanitize',action='store_true')
p.add_argument('--headers',type=Path,help='X11 public include directory; defaults to system headers')
a=p.parse_args();tests=Path(__file__).resolve().parent;src=tests.parent
cc=shlex.split(os.environ.get('CC','cc'))
flags=['-std=c99','-Wall','-Wextra','-Werror','-O1','-I'+str(src)]
if a.headers:flags+=['-I'+str(a.headers.resolve()),'-DNeedFunctionPrototypes=0']
if a.sanitize:flags+=['-g','-fsanitize=address,undefined','-fno-omit-frame-pointer']
with tempfile.TemporaryDirectory(prefix='xterm-font-cache-') as directory:
    out=Path(directory)
    subprocess.run([*cc,*flags,'-Dmalloc=test_malloc','-Dcalloc=test_calloc','-Dfree=test_free','-c',str(src/'font-cache.c'),'-o',str(out/'cache.o')],check=True)
    subprocess.run([*cc,*flags,str(tests/'font-cache.c'),str(out/'cache.o'),'-o',str(out/'check')],check=True)
    subprocess.run([str(out/'check')],check=True)
