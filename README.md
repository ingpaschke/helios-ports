# Helios ports

Native Transputer applications and libraries built from pinned upstream sources
and Helios patches. Each directory under `ports/` contains a readable
`port.json` recipe and an ordered set of focused patches. New source and support
files live in `files/`, at their paths relative to the upstream source root.
Patches contain changes to upstream files. Each patch names the problem it
solves; the shared builder downloads, verifies, applies patches and new files,
then compiles and stages the result. Source trees and generated files live under
`work/`.

To learn how to adapt a program, start with [the porting
walkthrough](docs/porting.md). Use [the recipe reference](docs/ports.md) when
writing your own recipe.

| Port | Version | Contents |
| --- | --- | --- |
| [x11](ports/x11/port.json) | X11R4, patch level 18 | X libraries, clients and demos |
| [fvwm](ports/fvwm/port.json) | 1.24r | Window manager, GoodStuff and FvwmPager |
| [irssi](ports/irssi/port.json), [glib](ports/glib/port.json) | 0.8.10 / 1.2.10 | IRC client and support library |
| [links](ports/links/port.json) | 2.30 | Text browser with HTTP and HTTPS |
| [slrn](ports/slrn/port.json), [slang](ports/slang/port.json) | 1.0.3a / 2.3.3 | Newsreader and terminal library |
| [xmahjongg](ports/xmahjongg/port.json) | 3.7 | Mahjong solitaire, also named xshanghai |
| [frotz](ports/frotz/port.json) | 2.55 | Z-machine interpreter |
| [nethack](ports/nethack/port.json) | 3.1.3 | NetHack with the X11 interface |
| [telnet](ports/telnet/port.json) | Helios port | Telnet client with terminal and window-size translation |
| [xterm-tty](ports/xterm-tty/port.json) | Helios terminal server | Native terminal service for xterm |

## Build

The host needs Python 3.12 or newer, Git, llvm-t800, a built Helios-NG SP2 SDK
and its assembler. Network clients and the desktop also need a built helios-unix
installation stage. The current recipes target T800 processors. X11 needs Bison
and Flex to generate twm's parser. NetHack additionally uses GCC, qemu-i386 and
an i386 musl host SDK for its ILP32 data generators; slrn needs `tic`, and Frotz
needs `make`.

```sh
python3 ports.py list
python3 ports.py show irssi
python3 ports.py prepare irssi
python3 ports.py build irssi \
    --sdk /path/to/sdk/prod/TRAN \
    --asm /path/to/sdk/host/bin/asm \
    --compat /path/to/helios-unix-stage
```

Dependencies are built automatically. Omit the port names to build everything;
add `--host-sdk /path/to/i386-musl-sdk` for NetHack. Use `--toolchain` to select
an llvm-t800 installation other than `/opt/llvm-t800`, `--jobs` to control
compilation parallelism, and `--work` to put all caches and outputs elsewhere.
`--offline` uses cached upstream inputs without downloading.

Each port has `work/<name>/src`, build logs, `stage` and `build-info.json`. The
stage is a tree to install below `/helios`. Distribution assemblers can merge
these stages and provide their own desktop configuration. Source-only
dependencies such as GLib and S-Lang are compiled by their consumer's recipe.

```sh
python3 ports.py verify irssi \
    --sdk /path/to/sdk/prod/TRAN \
    --asm /path/to/sdk/host/bin/asm \
    --compat /path/to/helios-unix-stage
```

Verification checks the recipes, tools, SDK, dependencies and output contents. A
normal build reuses verified results and rebuilds stale components. `--force`
reruns preparation, linking and installation, while unchanged compiled objects
may still be reused. Use a new `--work` directory for a completely clean build.

## Change or add a port

Edit a copy of the prepared source, then regenerate its patch:

```sh
python3 ports.py prepare irssi
cp -a work/sources/irssi work/irssi-edit
# Edit upstream files in work/irssi-edit.
python3 ports.py diff irssi --edited work/irssi-edit \
    --patch 05-my-change.patch --description "Describe the specific problem this fixes"
```

`diff` saves new files under `files/` automatically. `diff --patch NEW.patch`
appends a focused change to upstream files. Without `--patch`, it updates the
last patch while preserving earlier ones. The description and checksums are
recorded in `port.json`. Use a clean source copy for this step: the compilation
tree contains generated files that should not become source changes. Upstream
tests and our port regression tests belong beside their code under `files/` when
they are new, or in patches when they modify upstream tests. Build steps and
install paths belong in the recipe. Dependency headers come from their
dependency's build; generated code and data stay out of patches. To edit
supplied files directly, change `ports/<name>/files/...`, then run `python3
ports.py refresh <name>` to record the reviewed source checksums. See [the
recipe reference](docs/ports.md) for the configuration format.

## Runtime

Programs run on Helios. X clients need an X server and `DISPLAY`; terminal
applications use xterm's native terminal service. Network clients use
`/internet`. TLS-enabled clients use helios-unix's provider interface; the
supplied provider delegates TLS to the Linux I/O server. Install the matching
helios-unix libraries and DNS helpers with these clients.

Prepared sources retain upstream documentation and port-specific instructions.
Platform adapters are under `helios/` in applicable source trees, and can be
read directly in `ports/<name>/files/helios/` without downloading upstream.
NetHack's `README.md` describes its host SDK and game tests.

## Tests and licences

Run the build-tool regression checks with:

```sh
python3 -m unittest discover -s tests
```

Application tests remain with each port's reconstructed sources. The shared
build tooling is [MIT licensed](LICENSE.tools). Applications and their
adaptations retain their respective licences; see [LICENCES.md](LICENCES.md).