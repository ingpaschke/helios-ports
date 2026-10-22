# ncurses for Helios

This port provides ncurses 6.6, including the menu, panel and form libraries.
It builds native Transputer code and uses Helios terminal streams. It does not
require Linux, `/internet`, or the KTM I/O server at run time.

The initial profile uses static libraries and eight-bit characters. Wide
characters, Unicode, C++ bindings and native `tic`/`infocmp` executables are not
built. Existing applications linked with Helios's BSD curses are unaffected.

## Build and link

Use the shared builder with a Helios-NG SP2 SDK and a helios-unix stage:

```sh
python3 ports.py build ncurses --sdk /path/to/sdk/prod/TRAN \
    --asm /path/to/sdk/host/bin/asm --compat /path/to/helios-unix-stage
```

The host needs GCC, Make, Awk, and ncurses's `tic` and `infocmp`. Configure and
Make build the library; native host tools generate tables and terminal entries.
The recipe supplies cross-compilation answers for the Helios headers. In
particular, `bool` is an `int`, while `chtype` and `mmask_t` are 32-bit unsigned
integers. Public headers work with both the clang driver and `hcc`.

Install the stage below `/helios`. Applications should put
`/helios/local/include/ncurses` before the SDK's include directory and link
`ncurses.lib`, `helios-unix.lib`, then `bsd.lib`. For a cross build:

```sh
clang --target=t800-helios --sysroot=/path/to/sdk/prod/TRAN \
    -I/path/to/ncurses-stage/local/include/ncurses \
    -I/path/to/helios-unix-stage/include/helios-unix program.c \
    -L/path/to/ncurses-stage/local/lib -lncurses \
    -L/path/to/helios-unix-stage/lib -lhelios-unix -lbsd \
    -Wl,-s32768 -o program
```

The example reserves a 32 KiB stack. Ncurses needs more stack than the old
minimal curses adapter; Frotz uses the same allocation.

Link `panel.lib`, `menu.lib` or `form.lib` before `ncurses.lib` when needed. Do not
mix ncurses headers with the old BSD `curses.lib`. Frotz's recipe demonstrates a
full-screen application; Irssi uses only the terminfo functions.

## Terminal descriptions

The library searches `/helios/local/share/terminfo`. `TERMINFO` can select another
directory. The supplied `files/helios.ti` contains the Helios ANSI, VT100 and
X11R4 xterm descriptions, shared with S-Lang applications such as slrn. These
three entries are installed and also built into the library. Linux console,
screen, screen-256color and xterm-256color entries are compiled from the pinned
upstream database and require the installed files. Additional descriptions can be compiled
on the host with `tic -o destination descriptions.src` and copied to Helios.

`TERM` must describe the actual terminal. The X11R4 xterm is monochrome;
a modern xterm description advertising colors cannot add color support to it.
This port does not change existing users' `TERM` settings.

## Helios adaptations

`01-helios.patch` adds the target name and handles native header conventions,
queries dimensions through `hu_terminal_size`, and disables Helios cooked
output interpretation while ncurses controls the display. Ncurses retains its
normal SIGWINCH, KEY_RESIZE, window and pad handling.

`02-native-keys.patch` registers the native terminal server's C1 cursor,
editing and function-key sequences alongside the terminal's terminfo keys.
The small implementation is supplied in `files/ncurses/tinfo/heliosky.h`.
Helios terminal servers translate those keys even in raw mode, so simply
setting `TERM=xterm` is insufficient. Ordinary eight-bit text is not rewritten.
Applications using `keypad(window, TRUE)` receive standard `KEY_*` values.

## Test on a terminal

Build `tests/terminal.c` with the command above. Run it with an optional log
filename, then resize the terminal, press all four arrows, Home/End, F1/F2, Escape-1 and J, as well as other
letters, and press Q to exit. It checks key definitions, window/pad allocation,
terminfo parameter expansion and input decoding, and logs KEY_RESIZE with the
new dimensions. The transcript should show KEY_UP=259, KEY_F(1)=265 and
KEY_RESIZE=410. A missing or unsupported terminal description should fail with
a clear ncurses startup error rather than silently selecting another terminal.
Copy the log back and run `python3 tests/check_log.py ncurses.log` to check
these events. Test a non-fallback entry such as `screen` with `TERMINFO` pointing
at the installed database to verify file-based lookup too.

`tests/widgets.c` checks creation, posting and cleanup of panels, menus and
forms. Build it with `-lmenu -lpanel -lform` before `-lncurses`, then run it in a
terminal at least 24 columns wide. It prints a PASS/FAIL result and exits.
