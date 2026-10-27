# NetHack 5.0 for Helios

NetHack 5.0.1 with both the terminal and X11 tile interfaces. It uses the
shared X11R4 port, including its keyboard handling, and Helios FpLib for
floating point on T425 and T800/T805 processors. Stack checks are enabled.

## Build

```sh
python3 ports.py build nethack5 --sdk /path/to/sdk/prod/TRAN \
    --asm /path/to/sdk/host/bin/asm --compat /path/to/helios-unix-stage
```

The recipe uses NetHack's cross-build rules. Host tools generate `nhdat`
and `x11tiles`; a 32-bit host SDK is not needed. Lua and the BSD regular
expression engine are included in the reconstructed upstream sources.
The X11 dependency supplies the target headers and libraries. No SDK files
are patched or copied into the toolchain.

## Install and run

Merge `work/nethack5/stage` into `/helios`, including its relative launcher
symlink. The game directory must be writable.

```sh
/helios/local/bin/nethack5          # terminal interface
/helios/local/bin/nethack5 -wx11   # X11 interface with tiles
```

`DISPLAY` identifies the X server. X11 resources use class `NetHack5`,
installed in `lib/x11/app-defa/NetHack5`. The host preprocessor expands the
upstream resource template during the build. Both normal and shifted keys
use Xlib's keyboard translation, including keyboard-map changes.

Data, scores and saves live in `/helios/local/games/nh50`. These saves are
incompatible with older NetHack versions. The separate directory and
launcher preserve an existing NetHack 3.1.3 installation. Use `NETHACKDIR`
or `-d` to select another writable game directory.

Options come from `$HOME/nethack.cnf`, falling back to the playground's
`nethack.cnf`. Copy the supplied file to your home directory to customise it.
The defaults keep manual pickup and letter-key movement, enable full item
menus, pet and item-pile markers, a hit-point bar and low-health warnings.
X11 uses a character-creation dialog and a complete extended-command menu
on `#`. The new-player tutorial remains disabled. Press `?` for help,
`O` to change options or `S` to save; `nethack5 -s` lists high scores.
`sysconf` selects wizard-mode users.

The X11 resources use the desktop's ivory and teal colours, fixed-width
`9x15` text, six visible message lines, a separator between old and new
messages, highlighted prompts and automatic keyboard focus for popups.
Tiles retain their original colours. A persistent inventory window is
available with `perm_invent` (off by default); use `O` to enable it.
For larger tiles on a large display, set `NetHack5.double_tile_size: True`
in the X server's resources. This doubles the full map width to 2560 pixels.
Game options and X resources apply to newly started games; options stored
in a save may need to be changed through `O` after restoring it.

The target filenames follow the Atari 8.3 convention. The data archive
contains the Lua scripts; the target does not create individual Lua files.
The tty interface uses ncurses' termcap compatibility API and its installed
terminfo profiles, including `xterm` and `xterm-256color`.

## Native adaptations

`01-helios.patch` configures the runtime, locking, file names and cross-build.
`02-x11-tiles.patch` connects the shared X11 libraries, substitutes
`XtDatabase` for the later resource-manager accessor, adapts the viewport
callback and gives this version its own application resource class.
New compatibility files are in `files/`, including the bounded formatter,
native keyboard adapter and missing C99 declarations.
`03-tile-format.patch` keeps the host-generated tile header 32-bit and checks
its dimensions. `04-terminal.patch` reads native window dimensions and uses
the helios-unix keyboard decoder.
`05-status-columns.patch` reserves the two width slots used by fancy-status
columns, preventing their initialization from overwriting adjacent lists.
`06-terminal-capabilities.patch` links ncurses instead of Helios's minimal
termcap implementation, so ordinary xterms are recognized.
`07-terminal-size.patch` preserves Helios's window dimensions when entering
character-at-a-time input mode; the native fields are not Unix read timers.
`08-defer-redraw.patch` defers terminal redraw until a level is fully generated
or restored, avoiding reads of incomplete shop state.
`09-interface-defaults.patch` sets desktop colours, popup focus and prompt
and message-display defaults in the upstream X resource template.

## Checks

After building, check the tile generator's output on the host:

```sh
python3 work/nethack5/src/tests/test_tiles.py
```

The header-size check also compiles with the target as part of NetHack.
Both interfaces should be checked at runtime: character creation, input,
window resizing, saving and restoring; X11 should display the tile map.

## Licence

See `dat/license` and the notices in `nhlua/` and `sys/amiga/regex/COPYRIGHT`.
