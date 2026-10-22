#!/usr/bin/env python3
"""Check the real target level writer's header against its reader's ABI.

Helios widens even declared char parameters to int. Serializing sizeof(lev)
therefore wrote four bytes while getlev read one; host-only tests miss this.
"""
import re
import sys
from pathlib import Path

ir = Path(sys.argv[1]).read_text()
body = re.search(r"^define[^\n]*@savelev\([^\n]*\{(.*?)^}", ir, re.M | re.S)
assert body, "missing target savelev definition"
writes = re.findall(r"call void @bwrite\([^\n]*, i32(?: noundef)? (\d+)\)", body[1])
assert writes[:2] == ["4", "1"], f"level header must be int PID + one-byte level, got {writes[:2]}"
print("PASS: target savelev writes four-byte PID and one-byte level number")
