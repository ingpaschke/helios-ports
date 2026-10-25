# XV for Helios

This port builds XV 3.10a as a native Transputer X client. It uses the shared
X11R4 port and installs `xv`, `bggen`, `xcmap` and `xvpictoppm` under
`/helios/bin/x11`.

## Build and install

```sh
python3 ports.py build xv --sdk /path/to/sdk/prod/TRAN \
    --asm /path/to/sdk/host/bin/asm --compat /path/to/helios-unix-stage
```

Install `work/xv/stage` and its X11 dependency stage below `/helios`. Man pages
and the upstream README are in `/helios/local/share/xv`.

## Run

`DISPLAY` must name a reachable X server. Start it from a graphical session:

```sh
/helios/bin/x11/xv &
```

XV opens files named on the command line or its file dialog. It uses `TMPDIR`,
or `/helios/tmp`, for temporary image and background files. `bggen` must remain in
`PATH`; the graphical login's default path already includes `/helios/bin/x11`.

## Image formats

The port includes the bundled JPEG and TIFF libraries and XV's readers for GIF, PBM/PNM, BMP, Sun raster, XBM,
XPM, PCX, IFF/ILBM, Targa, XWD, FITS, Utah RLE and IRIS. PNG is not supported by this XV version. PDS and
PostScript input are disabled as well, because they depend on external helper
programs. Compressed `.Z` input uses `uncompress` from `PATH` when that
program is installed. It reads the input through stdin, without renaming it;
missing or failing helpers leave the input intact and remove partial output.

## Native adaptations

`files/helios` supplies what xv expects from a Unix C library:

- `mktemp.c` supplies a private temporary-file adapter. It uses native task
  ports and machine names rather than Helios's process-local POSIX PIDs,
  and reserves each returned name with exclusive creation. FITS retains its
  basename reservation until its page files are removed.
- `files.c` removes browser directories through filesystem calls without
  following symbolic links, and starts `uncompress` with redirected file
  descriptors. Filenames are passed without shell interpretation.
- `platform.c` refuses `mknod` for device and FIFO nodes. Helios has no such
  filesystem objects; native channels replace them.
- `xv-platform.h` declares those functions and `select()`, which Helios declares
  in `<sys/socket.h>`.

The source patch stops upstream's `xv.h` from including Helios's legacy
`malloc.h`, whose K&R declaration conflicts with its ANSI `stdlib.h`.

XV calls `system()` for background generation with `bggen` and explicit pipe
commands. Helios provides `system()` and a shell. Copying special files in the
browser fails with a warning rather than creating a device node.

## Regression checks

The host checks exercise native task identity with stubbed SDK calls and use
the host filesystem and subprocesses. They include concurrent temporary-file
allocation, FITS reservation cleanup, browser directory moves with shell
metacharacters in their names, and decompression success and failure paths:

```sh
python3 ports.py prepare xv --offline
python3 work/sources/xv/tests/test_files.py
```

These checks need a host C compiler. They do not require a running X server.

## Licence

XV is shareware, not free software. Its copyright notice permits copying and
distributing unmodified copies for non-commercial purposes, but modified
versions may not be distributed without the author's prior consent. This port's
patch and adapter files therefore need that consent before publication. The
upstream notice is installed as `local/share/xv/copyright.h` and its terms are
reproduced in the upstream `README`.
