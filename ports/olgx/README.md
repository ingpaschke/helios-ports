# OLGX for Helios

OLGX is Sun's Open Look graphics library. This port builds native Transputer
code against the shared X11R4 port. It provides drawing routines and public
headers; it does not require the XView toolkit.

```sh
python3 ports.py build olgx --sdk /path/to/sdk/prod/TRAN \
    --asm /path/to/sdk/host/bin/asm --compat /path/to/helios-unix-stage
```

The stage installs `local/lib/olgx.lib` and `local/include/olgx/olgx.h` below
`/helios`. Link OLGX before Xlib. The initial profile uses eight-bit text and
the original bitmap glyph fonts. The X server must have the Open Look fonts
in its own font path; a remote display reads fonts on the display computer.

The source is pinned to the preserved XView 3.2p1.4 tree. The patch replaces
obsolete allocator declarations with `<stdlib.h>`. The original Sun licence
is installed under `local/share/olgx/LEGAL_NOTICE`.
The preserved source repository licenses its later changes under GPLv3;
that text is retained as `COPYING.GPL3`. New native adapters retain their
individual notices.
