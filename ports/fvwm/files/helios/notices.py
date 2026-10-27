# SPDX-License-Identifier: MIT
"""Collect the upstream component notices for the binary installation."""
from pathlib import Path
import re
import sys

notices = []
for name in ['fvwm/fvwm.c', 'modules/GoodStuff/GoodStuff.c', 'modules/FvwmPager/FvwmPager.c']:
    text = Path(name).read_text()
    notices.append(name + '\n' + text[:re.search(r'^#', text, re.M).start()].strip())
Path(sys.argv[1]).write_text('\n\n'.join(notices) + '\n')
