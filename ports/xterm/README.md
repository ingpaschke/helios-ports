# xterm 412 for Helios

A native terminal with UTF-8, 256 colours, scrollback, selection, paste and
mouse reporting. It uses the X11R4 libraries and the native `xterm-ttyserv`
service. The executable is `xterm412`, with resource class `XTerm412`, so it
can be installed alongside the X11R4 terminal.

```sh
/helios/bin/x11/xterm412
/helios/bin/x11/xterm412 -geometry 100x32 -e /helios/bin/ssh user@host
```

`DISPLAY` must identify a reachable X server. The child receives
`TERM=xterm-256color`; install the ncurses port's terminfo files too.
For legacy termcap applications, append `local/share/xterm/helios.tc` to
`/helios/etc/termcap`, or start the terminal with `-tn xterm`.
Resizing updates the native terminal attributes and signals its foreground
application through the terminal service.

The supplied `XTerm412` application defaults select eight-bit ISO-8859-1 text
for native Helios applications and core Unicode bitmap fonts, with matching
normal, bold and double-width fonts. The display
host needs these fonts; Debian and Ubuntu supply them in `xfonts-base`.
Include `/usr/share/fonts/X11/misc` in the X server's font path. Missing glyphs
are limited by the selected font, including characters outside its repertoire.
Use `-u8` to start in UTF-8 mode, or `+u8` to select eight-bit mode explicitly.
Applications can switch with ESC % G (UTF-8) and ESC % @ (eight-bit).
The Helios SSH client enables UTF-8 for interactive connections and restores
the terminal's previous encoding on exit. No character conversion is applied
to redirected SSH output.

## Build

```sh
python3 ports.py build xterm --sdk /path/to/sdk/prod/TRAN \
    --asm /path/to/asm --compat /path/to/helios-unix-stage
```

The shared builder builds the X11, ncurses and terminal-service dependencies.
`helios/prepare.py` generates the upstream parser constants and combines the
upstream application defaults with `helios/XTerm.resources`. Target settings
are explicit in `xtermcfg.h`; no host feature probes determine target settings.

The two patches connect native startup, terminal packets and resizing, and
adapt mouse translations to X11R4. `helios/helios-pty.c` implements the native
terminal transport. `helios/xlib.c` supplies the missing X11R5 atom, selection,
gravity and colour interfaces. UTF-8 selection conversion and character-width
calculations use upstream xterm code.

XIM, Xft, session-management extensions, Tektronix graphics and SIXEL are
not enabled. X11R4 cannot inspect custom Xt translations to infer their mouse
modifier policy; ordinary selection and application mouse reporting remain
available. The supported clipboard encodings are STRING and UTF8_STRING;
COMPOUND_TEXT conversion is limited to the native single-byte locale.

## Tests

The build also produces `work/xterm/tests/terminal-check`; it is not installed.
Run it in the new terminal with a writable log-file argument. It checks cursor
replies after BEL- and ST-terminated control strings, UTF-8 character widths,
including UTF-8 continuation bytes that overlap native C1 controls. It
then records resize events and keyboard bytes until Ctrl-D. The host test
runner under `helios/tests/` drives an isolated emulated Helios and a private
X server, including real menu callbacks under Caps Lock and Num Lock.
For example, after building the port:

```sh
python3 work/xterm/src/helios/tests/run.py \
    --image /path/to/isolated-helios-image \
    --template /path/to/t4-host.con --ioserver /path/to/native-ioserver \
    --binary work/xterm/bin/xterm412 --probe work/xterm/tests/terminal-check \
    --defaults work/xterm/src/XTerm412 --output work/xterm-test
```

The host needs `t4`, Xvfb, Python Pexpect, Pillow, Xlib and XTest. Use
`--cpu s4` to repeat on the emulated T425. The supplied image must be isolated:
the test boots it and writes temporary files under its `tmp` directory.

See upstream `COPYING` and the individual source notices for licences.
The new native adapter files are MIT licensed.
