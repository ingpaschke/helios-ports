# Nonpareil

Nonpareil 0.79 runs natively on Helios as `nonpar`. It simulates the original
Nut processor and ROMs of the HP-10C, HP-11C, HP-12C, HP-15C and HP-16C.
The default model is HP-15C.

```sh
nonpar
nonpar 12c
nonpar 16c
```

The X11 front end uses the original calculator faces, LCD segments and
annunciators. Press keys with the mouse, or use digits, arithmetic operators,
Enter, Backspace and the numeric keypad. The `f` and `g` keys select the
calculator's shift functions; Escape operates ON. F10 or the window manager's
close command exits. These calculators use RPN: `2 Enter 3 +` displays `5`.

Registers, stored values and programs are saved on a normal exit to
`$HOME/np15c.nst`, or the corresponding file for the selected model.
Use `-state FILE` for separate memories, or `-nostate` to start fresh.
The state format is readable text and is specific to this front end.
An interrupted file without its final `end` record is rejected.

Calculator data is installed under `/helios/local/share/nonpar`. `-data DIR`
selects another data root; `-display DISPLAY` overrides the X display.
The executable and installed data names follow Atari's 8.3 convention.

The GTK interface, other calculator families, plug-in modules, printer,
card reader and sound are not included. The simulator does not need
`/internet` for calculation; Xlib supplies the normal Helios display transport.

## Build

Use the shared port builder with a Helios-NG SP2 SDK and assembler. Pillow is
needed on the build host to convert the original PNG artwork. Runtime assets
are standard PPM/XBM images, XML calculator definitions and text keyboard maps;
the Transputer does not need GTK, GLib, PNG or libxml2.

```sh
python3 ports.py build nonpareil --sdk /path/to/sdk/prod/TRAN \
    --asm /path/to/sdk/host/bin/asm --compat /path/to/helios-unix-stage
```

The recipe builds X11, Expat and nanoprintf as dependencies. `--cpu t425`
builds the software floating point variant; the default is T800. Expat reads
the same calculator elements as upstream. Nanoprintf supplies bounded
formatting missing from the native C library.

`01-proc.patch` makes command dispatch and display callbacks synchronous when
`NP_SINGLE_THREAD` is selected. The X11 event loop schedules simulation work;
there are no GLib queues or simulator threads. `02-calcdef.patch` selects the
Expat adapter while retaining upstream's calculator-definition handlers.
New front-end, asset-conversion and platform files are under `files/helios`.
The asset converter verifies upstream ROM checksums before expanding them.

The input queue gives the simulated key scanner time to observe both press
and release, even when X11 batches the events. It is bounded; a full queue
temporarily stops reading further X events. The native microsecond timer is
extended across its 32-bit wrap. The calculator face is stored as an X pixmap,
and only changed LCD contents are redrawn during ordinary use.

## Checks

On Helios, each model can check ROM execution and optional state restoration
without opening a window:

```sh
nonpar 15c -test
nonpar 16c -test -state /helios/tmp/np16c.nst
```

The Linux reference check builds the same front end and simulation sources.
It requires a C compiler and the X11 and Expat development libraries:

```sh
python3 ports/nonpareil/tests/run_host.py --work work \
    --output /tmp/nonpareil-check --sanitize
```

`tests/x11_driver.c` drives a dedicated calculator display and captures its
window as PPM. Compile it with `cc ... -lX11`; `key` sends an ordinary held
key, `tap` batches press and release, `click X Y` presses a calculator button,
and `close` sends the window manager close request.

Upstream copyright, artwork credits and per-file licence notices are retained.
The installed `COPYING`, `CREDITS` and `LICENSES` accompany the calculator data.
