# NetHack 3.1.3 for Helios

Native T425 and T800/T805 builds using the X11R4/Athena interface with coloured
characters. The port includes game data, fonts, saves, scores and the recovery
utility. A tty interface, Unix shell escapes, mail delivery and external
compression are not supported.

## Build

The build requires llvm-t800, a Helios SDK and assembler, the X11R4 sources,
and a little-endian ILP32 host compiler for data generators. Those generators
write native structures, so a 64-bit host data layout is unsuitable. The build
checks their structure sizes and offsets against the target compiler.

Run in a working copy: the data generators write into `include`, `src` and
`dat`. For an i386 musl host toolchain, first run:

```sh
tools/bootstrap-host.sh /path/to/musl-source
```

This requires GCC, GNU binutils and i386 compiler-rt builtins. `HOST_BUILTINS`
selects the builtins archive; the default is LLVM 21's installed archive.
`qemu-i386` runs the host tools by default.

Build with explicit SDK locations:

```sh
python3 tools/build.py --cpu t800 \
  --clang /opt/llvm-t800/bin/clang \
  --asm /path/to/sdk/host/bin/asm \
  --sysroot /path/to/sdk/prod/TRAN \
  --runtime /opt/llvm-t800/sysroots/helios/lib/llvmrt-generic.lib \
  --x11-source /path/to/ports-work/x11/src \
  --x11-include /path/to/ports-work/x11/include \
  --x11-library /path/to/ports-work/x11/lib
```

`--sysroot`, `--asm` and `--x11-include` are required. The compiler and generic runtime default
to the installed `/opt/llvm-t800` toolchain; `CLANG` can override the compiler.
Use the source, headers and libraries produced by the shared X11 recipe.

A working 32-bit GCC/libc installation can instead use
`--host-cc 'gcc -m32' --host-runner ''`. Build CPU variants serially because
they share generated data. Supplied parser sources avoid a flex/bison dependency.

Output is staged under `build/<cpu>/stage`, with build logs and checksums
beside it; the binary archive is `dist/nh313-helios-<cpu>.tar.gz`.
The shared `ports.py build nethack` entry point prepares dependencies and
supplies these paths automatically.

## Install and run

Merge the staged `local` and `lib` directories into the Helios root, preserving
existing saves and scores. The game directory and its save and score files
must be writable by the player. See [installation instructions](sys/helios/README.txt).

With `DISPLAY` set to the X server, run:

```text
/helios/local/games/nh313/nethack -u Player
```

`S` saves; restart with the same player name to restore. `-s` lists scores.
Settings come from `$HOME/nethack.cnf`, the game's default configuration or
`NETHACKOPTIONS`. Application resources are installed as
`/helios/lib/x11/app-defa/NetHack`. The `nh10` and `ibm` SNF fonts are for a
Helios X server; a Linux display can use its `fixed` font with ASCII symbols.

The level format stores one byte for the level number. Saves and level files
from builds with a different layout are incompatible; keep backups when
changing builds. Installed game filenames follow the DOS 8.3 convention.

## Checks

The build checks target and host data layouts, date formatting, DOS filenames,
file-lock behaviour, package filename collisions, X resources and font glyphs.
`tools/smoke.py --help` describes the optional emulator/X-server gameplay test
and its configurable dependencies.

## Licence

See [the NetHack licence](dat/license) and individual source notices.
The random generator includes software developed by the University of
California, Berkeley; its notice is in `sys/share/random.c`.
