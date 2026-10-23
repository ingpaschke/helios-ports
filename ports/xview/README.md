# XView and Open Look clients for Helios

This port builds the XView 3.2 toolkit as a native Transputer library, with
four applications:

| Program | Purpose |
| --- | --- |
| `olclock` | Open Look analogue or digital clock |
| `textedit` | Graphical text editor with file dialogs, backups and selection filters |
| `shelltool` | Sun terminal emulator |
| `cmdtool` | Terminal with text scrollback |

It uses the shared X11R4 and OLGX ports. It does not provide the proprietary
OpenWindows DeskSet applications, ToolTalk, Sun RPC services or NeWS.
The toolkit uses the C locale; this build does not enable its wide-character
internationalization option.

## Build and install

```sh
python3 ports.py build xview --sdk /path/to/sdk/prod/TRAN \
    --asm /path/to/sdk/host/bin/asm --compat /path/to/helios-unix-stage
```

Install `work/xview/stage` and its dependency stages below `/helios`. Programs
are in `/helios/bin/x11`. Public headers and `xview.lib` are under
`/helios/local/include` and `/helios/local/lib`. `regexp.lib` supplies file
chooser filters. The ncurses dependency supplies the Sun terminfo entry.
`local/share/xview/sun.tc` can be appended to a distribution's `/helios/etc/termcap`
for older applications that use termcap.

Run the clients in an existing graphical login, for example:

```sh
/helios/bin/x11/olclock &
/helios/bin/x11/textedit /helios/users/guest/notes.txt &
/helios/bin/x11/shelltool &
/helios/bin/x11/cmdtool &
```

`DISPLAY` must name a reachable X server. The display computer needs the
X11R4 Open Look glyph/cursor and Lucida bitmap fonts. These applications also
work under other window managers. Their manuals and licence notices are in
`/helios/local/share/xview`.

Help text is installed in `/helios/local/openwin/lib/help`; `HELPPATH` can
select another directory. The separate `olwmslave` help viewer is not included.

## Editor

Textedit supports loading, editing, repeated saves, backup files ending in
`%`, file dialogs and the X selection clipboard. Meta-S saves the current
file; the File menu provides the same operation. On typical Linux keyboards,
Meta is Alt. Open Look buttons open menus with button 3.

The Extras menu contains native `sort`, reverse sort and `uniq` filters.
Override it with the `text.extrasMenuFilename` resource or `EXTRASMENU`.
The installed default is `/helios/local/openwin/lib/textmenu`.
Filters are started with `hu_spawnp` and native message channels. Their
standard input and output are set through descriptor actions; they do not
rely on a child sharing the editor's memory.

Saving waits for Helios writes to complete. It does not provide Unix `fsync`
durability guarantees. Backup identity checks use resolved native stream
names, because the native POSIX `stat` interface has no inode numbers.

## Terminals

Shelltool and Cmdtool use `/helios/lib/xterm-ttyserv`, also supplied by the
shared terminal port. Local terminal communication uses native Helios messages.
Their child processes receive `TERM=sun`; keep the Sun terminfo and termcap
entries installed. Resizing updates native terminal dimensions and delivers
SIGWINCH through the same terminal service used by xterm.

Cmdtool keeps text scrollback and leaves canonical line editing and echo to
Helios. It does not provide Sun's editable command-history mode: that requires
a record-mode PTY, which the native terminal service does not implement.
Full-screen programs can switch to terminal mode; Shelltool is the simpler
choice for these programs.

## Port structure

`port.json` selects the upstream sources, compilation groups and installed
files. Numbered patches modify existing upstream files; new native code is in
`files/helios`:

- `notifier.c` supplies readiness waits and real-time notifier timers.
- `terminal.c` connects the toolkit to the shared native terminal backend and
  reads each packet's header and payload together.
- `system.c` compares native file names without assuming Unix inode identity.
- `process.c` launches editor filters and cleans up failed launches.
- `headers.py` lays out public and private headers from the upstream Imakefiles.

Notifier reads, readiness and `fcntl` wrappers are private to XView. They do
not replace the application's libc or Xlib functions. Real timers run through
the notifier; CPU-time interval timers are unavailable.

For another XView port, compile with the native SDK, `-Dhelios -D_BSD` and the
staged headers. Link `xview.lib`, `regexp.lib`, `olgx.lib`, Xext, Xlib,
helios-unix and the native SDK libraries as shown in this port's targets.

## Regression checks

Build the native adapter checks after building the port:

```sh
python3 ports/xview/tests/build.py --work work --sdk /path/to/sdk/prod/TRAN \
    --asm /path/to/sdk/host/bin/asm --compat /path/to/helios-unix-stage
```

Copy `work/xview/tests/adapters` to Helios and run it with an existing writable
test directory as its only argument. It checks allocating current-directory
lookup, public headers, timers, distinct-file identity, aliases, descriptors
beyond the notifier mask, filter input/output/EOF and failed-launch cleanup.
It creates and removes files named `identity.a`, `identity.b` and
`identity.missing` in that directory.

The second probe, `work/xview/tests/chooser`, takes an existing directory
with `DISPLAY` set. It creates the Open dialog and exits after two seconds;
this catches failures in allocating directory lookup and dialog initialization.

In an isolated display, also test typing, changing focus, editor load/save,
reopening a file with an existing backup, terminal commands and resizing a
SIGWINCH-aware program. Test Cmdtool and Shelltool separately, then close the
clients and confirm that their child tasks and terminal services exit.

`events` runs with `DISPLAY` set and checks that canvas event translation
returns caller-owned events that remain valid across subsequent calls.

`clock-oom` links the actual clock with a test-only allocation hook. Run it
three times with `XV_FAIL_ALLOC` set to `1`, `2`, and `3`. Each run must report
`FAILURE_INJECTED`, print `clock: out of memory`, and exit with status 1.
The hook is not included in the installed applications.

To check terminal labels and login commands, run Cmdtool and Shelltool with
`-L` and a command path longer than 49 characters, then with a frame label
longer than 150 characters (`-Wl`). The command must start, and icon labels
must remain terminated and no longer than 29 bytes.

The `terminal` test launcher supplies the long arguments without depending on
console input-line limits. With `DISPLAY` set, run it as follows and repeat
with `cmdtool`:

```sh
terminal shelltool
terminal shelltool label
```

Before running it, copy the launcher itself to
`/helios/tmp/xview-fixes/very-long-login-command-directory-to-exercise-buffer-bounds/xvchild`.
It then also acts as the child command and records successful login-style
execution in `/helios/tmp/terminal-child.log`. The second invocation supplies
a 300-byte frame label. The child exits after 40 seconds.
