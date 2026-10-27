# Frotz for Helios

Frotz 2.55 uses the ncurses port for its normal Unix terminal interface,
including pads, cursor keys and resizing. The recipe builds ncurses first;
the interpreter reserves a 32 KiB initial stack for ncurses.
`dfrotz` retains the plain-text interface and does not link curses.

The patches cover build declarations, Helios paths and eight-character default
save names, the native `select` declaration, and a V1–V3 status-line height bug
when resizing. There is no separate curses emulator or input loop in this port.

Run `frotz /path/to/story.z5`. `ZCODE_PATH` or `INFOCOM_PATH` can name a search
path; the Helios default is `/helios/local/games`. Set `ESCDELAY` to change the
ncurses escape-key timeout (Frotz defaults to 50 milliseconds).

Helios reads settings from `$HOME/frotz.cnf`, falling back to
`/helios/etc/frotz.cnf`. Both names work on Atari 8.3 filesystems.
Use the builder's `--cpu t425` option for a T425-compatible interpreter.
