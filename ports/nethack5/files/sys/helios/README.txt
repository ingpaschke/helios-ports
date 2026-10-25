NetHack 5.0 for Helios
======================

Start the terminal interface with:
    /helios/local/bin/nethack5
Start the X11 tile interface with:
    /helios/local/bin/nethack5 -wx11
DISPLAY must identify the Linux X server when using X11.

Game data, scores, locks and saves live in /helios/local/games/nh50.
NETHACKDIR or the -d option selects another writable playground.
This version's saves are separate from NetHack 3.1.3 and are incompatible
with that version. Do not point both games at the same directory.

Options come from $HOME/nethack.cnf, falling back to nethack.cnf in the
game directory. Use h/j/k/l/y/u/b/n to move, ? for help and S to save.
The X11 interface supports mouse movement, menus and coloured tiles.
Its application resource class is NetHack5. The tile file is x11tiles.

The tty interface uses ncurses terminal profiles. An xterm should have
TERM=xterm or xterm-256color; install the ncurses port's terminfo data.

All files created on the target use 8.3 names for Atari filesystems.
The executable runs on T425 and T800/T805 processors; floating-point
operations use Helios FpLib. Stack overflow checks are enabled.

See license for the NetHack licence and man nethack5 for game options.
