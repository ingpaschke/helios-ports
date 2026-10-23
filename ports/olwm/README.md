# Open Look window manager for Helios

This port builds Sun's OLWM as a native Transputer program using OLGX, Xlib
and Xext. It supports Open Look window frames, workspace and window menus,
pinned menus, icons, moving and resizing, and launching native Helios tasks.
The full XView toolkit, `olwmslave` help viewer and NeWS/PostScript actions are
not included.

```sh
python3 ports.py build olwm --sdk /path/to/sdk/prod/TRAN \
    --asm /path/to/sdk/host/bin/asm --compat /path/to/helios-unix-stage
```

Install the stage below `/helios`. OLWM is `/helios/bin/x11/olwm`; the native
graphical-session launcher is `/helios/bin/x11/olwm-session`. Libraries and
headers come from the `x11` and `olgx` dependencies.

## Configuration and session startup

`OPENWINHOME` defaults to `/helios/local/openwin`. OLWM reads its menu from
`$OPENWINHOME/lib/openwin-menu` and resources from
`$OPENWINHOME/lib/app-defaults/Olwm`. A user's `$HOME/.openwin-menu` and X
resources can override the defaults. The port supplies the upstream menu as
an example; distributions should supply commands for their installed tools.

Use `olwm-session` as the desktop command in an XDM session script. It starts
OLWM through that login's TFM and waits for it. Applications launched by menus
inherit that TFM and their display's environment. The original session owner
stays alive while a replacement manager runs after a restart.

The display needs Open Look glyph and cursor fonts plus Lucida fonts. Add
the X11R4 `misc`, `75dpi` and `100dpi` directories to the Linux X server's font
path. These paths refer to files on the display computer.

Useful resources for a remote display are:

```text
OpenWindows.ServerGrabs: False
OpenWindows.DragWindow: False
OpenWindows.RunSlaveProcess: False
```

Turning off server grabs lets other clients keep drawing while menus are
active. Outline moves and resizes still protect their XOR drawing with server
grabs until the button is released. If a legacy core keymap clears Num Lock's mapping
but XKB retains its locked Mod2 state, also use:

```text
OpenWindows.Modifier.Ignore: Lock,Num_Lock,mod2,mod5,Mode_switch
```

On other keymaps, configure ignored modifiers to match their assignments.

## Adaptations

The patches are divided by purpose:

1. Native task launching, signal handling and readiness waits. Menu commands
   run through `hu_spawn`; `hu_poll` waits on the X connection. Unsupported
   optional help and NeWS actions report their limitations.
2. Standard C headers and the non-internationalized build. Helios's
   `<memory.h>` describes nucleus memory objects, so Unix string operations
   use `<string.h>`.
3. Native timed waits and full subsecond event deadlines.
4. Correct child `DISPLAY` construction and release of the old X connection
   before restarting the manager.
5. Passive grabs for every configured ignored-modifier combination, and
   release of unbound clicks so a synchronous grab cannot freeze input.
6. Thaw decoration input before chord detection so passive focus grabs cannot
   delay dragging. Replay pane clicks to applications; retain the motion
   that starts a drag and the final button-release position.
7. Replay intercepted presses to resize corners and window controls before
   thawing title-bar input, preserving their own button handlers.

New native adapters are readable source files in `files/helios`. The Sun
licence and OLWM manual are installed in `/helios/local/share/olwm`.
The preserved source repository's later patches are GPLv3; its licence text
is included as `COPYING.GPL3`. New native adapters are MIT licensed.

## Runtime checks

The child-display regression test runs on the build host without an X server:

```sh
cc -std=gnu89 -Iwork/sources/olwm/clients/olwm -Iwork/x11/include \
    ports/olwm/tests/environment.c -o /tmp/olwm-environment-test
/tmp/olwm-environment-test
```

Test in a separate display. Open the workspace menu repeatedly with button 3,
cancel outside it, pin a submenu, launch a terminal and a graphical client,
and type in both after changing focus. Move and resize their frames. Repeat
with Num Lock's lock state active, and with an unbound modifier held during
a click. Input must continue after releasing it.

Restart OLWM from the workspace menu. Existing clients should survive and
receive new frames. Launch another terminal to check that the child display
address remains valid. Finally log out and verify that the test graphical
session ends. `ps all` should show menu-launched tasks in the same TFM session.

For an automated drag check, start OLWM, Calculator (`xcalc`) and `xmahjongg`
on an isolated display, then run on the display host:

```sh
python3 ports/olwm/tests/drag.py --display :21
```

The test covers clients with and without explicit focus hints and includes
a drag with only one motion event. Repeat with `OpenWindows.DragWindow: True`
and add `--solid` to verify that the first motion moves the window before
the button is released. The default five-pixel drag threshold is required.
Do not run this on a desktop someone is using.

To check passive-grab latency, set `OpenWindows.MouseChordTimeout: 2000`
in the isolated manager's resources and use `--solid --max-start-delay 1`.
Movement must begin without waiting for that two-second chord deadline,
including when the test switches focus away from Calculator before dragging.

For an automated resize check, start an xterm with
`-geometry 40x12+200+200 -title Resize-test` on the isolated OLWM display:

```sh
python3 ports/olwm/tests/resize.py --display :21
```

This checks all four corners with and without keyboard focus. Each drag
contains one motion event; the window must grow while its opposite corner
stays fixed.
