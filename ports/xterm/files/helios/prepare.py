#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Generate the parser tables from upstream definitions."""
from pathlib import Path
for base in ("VTparse", "Tekparse"):
    names=[s.split()[0] for s in Path(base+".def").read_text().splitlines() if s.startswith("CASE_")]
    Path(base+".hin").write_text("".join(f"#define {name} {n}\n" for n,name in enumerate(names)))
    Path(base+".cin").write_text("".join(f'{{ {n}, "{name}" }},\n' for n,name in enumerate(names)))

Path("XTerm412").write_text(Path("XTerm.ad").read_text()+"\n"+Path("helios/XTerm.resources").read_text())
