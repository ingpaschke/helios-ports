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
| [x11](ports/x11/README.md) | X11R4, patch level 18 | Native shared X libraries, VT-only terminal, clients and demos |
| [x11-support](ports/x11-support/README.md) | Helios BSD | Shared support module for X11R4 |
| [xterm](ports/xterm/README.md) | 412 | UTF-8 and 256-colour terminal, installed as xterm412 |
| [fvwm](ports/fvwm/port.json) | 1.24r | Window manager, GoodStuff and FvwmPager |
| [irssi](ports/irssi/port.json), [glib](ports/glib/port.json) | 0.8.10 / 1.2.10 | IRC client and support library |
| [links](ports/links/port.json) | 2.30 | Text browser with HTTP and HTTPS |
| [slrn](ports/slrn/port.json), [slang](ports/slang/port.json) | 1.0.3a / 2.3.3 | Newsreader and terminal library |
| [xmahjongg](ports/xmahjongg/port.json) | 3.7 | Mahjong solitaire, also named xshanghai |
| [ncurses](ports/ncurses/README.md) | 6.6 | Curses, terminfo, menu, panel and form libraries |
| [olwm](ports/olwm/README.md), [olgx](ports/olgx/README.md) | 3.2p1.4 | Open Look window manager and graphics library |
| [xview](ports/xview/README.md) | 3.2p1.4 | XView toolkit, Open Look clock, editor and terminals |
| [regexp](ports/regexp/README.md) | Helios implementation | Regular expressions for XView file chooser filters |
| [frotz](ports/frotz/port.json) | 2.55 | Z-machine interpreter |
| [nethack](ports/nethack/port.json) | 3.1.3 | NetHack with the X11 interface |
| [nethack5](ports/nethack5/README.md) | 5.0.1 snapshot | NetHack with terminal and X11 tile interfaces |
| [xv](ports/xv/README.md) | 3.10a | Image viewer with JPEG, TIFF and classic bitmap formats |
| [telnet](ports/telnet/port.json) | Helios port | Telnet client with terminal and window-size translation |
| [xterm-tty](ports/xterm-tty/port.json) | Helios terminal server | Native terminal service for xterm |

## Build

The host needs Python 3.12 or newer, Git, llvm-t800, a built Helios-NG SP2 SDK
and its assembler and AMPP tools. Network clients and the desktop also need a built helios-unix
installation stage. Builds default to T800; use `--cpu t425` for T425-compatible
code or `--cpu generic` for the generic Transputer target. X11 needs Bison
and Flex to generate twm's parser. NetHack additionally uses GCC, qemu-i386 and
an i386 musl host SDK for its ILP32 data generators. Frotz needs `make`; ncurses also needs GCC, Awk,
`tic` and `infocmp`.

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

## CPU and runtime releases

Use `--cpu t425` or `--cpu t800` and `--linkage static` or `--linkage shared`.
Shared is the default. Both choices are part of build reuse checks; keep
independent work directories when retaining several variants.

Profiles in `profiles/` select applications, CPU, linkage and installation
names. `atari-minimal` uses static libraries on T425; `atari-expanded` uses
shared libraries and adds the modern terminal. `ktm` selects T800 and shared
libraries. Boot maps, menus, user configuration and host services belong to
the distribution, rather than the ports archives.

```sh
python3 ports.py build --profile atari-minimal --work work/atari \
  --sdk /path/to/sdk/prod/TRAN --asm /path/to/sdk/host/bin/asm \
  --compat /path/to/helios-unix-t425/stage
python3 ports.py package --profile atari-minimal --work work/atari \
  --sdk /path/to/sdk/prod/TRAN --asm /path/to/sdk/host/bin/asm \
  --compat /path/to/helios-unix-t425/stage \
  --release 2026.10 --output release/hp425.zip
```

The KTM profile additionally needs `--host-sdk` for NetHack's data generators.
Use the same build arguments for `package`; it verifies existing builds and
never silently rebuilds them. Supplying port names packages that selection
and its dependencies instead of the full profile.

Archives contain a `helios/` runtime tree, `ports.jsn`, `sha256.txt` and install
instructions. On Atari copy `helios/` contents into the physical system folder
selected by your launcher. Paths are checked for 8.3 compatibility and case
collisions; known asset names are shortened with collisions rejected.
KTM archives use tar.gz and retain the normal Helios names. The expanded
Atari profile supplies XTerm412 resources for native bitmap fonts; the
configuration file is explicit under profiles/atari/. Symlinks to host
build files are materialized. Headers, import archives, regression programs
and source bundles are excluded from runtime releases; they remain in the
source checkout and build stages. Shared-module IDs and ABI versions must
match their clients, and missing port libraries reject packaging.

Build helios-unix with its own `build.py` for the same CPU before building the
ports. Network applications require `/internet`; the Atari minimal selection
uses local X and FIFO services. TLS and SSH additionally require a suitable
provider. Each port retains its upstream licence and included notices.

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
