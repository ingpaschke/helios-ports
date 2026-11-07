# FVWM for Helios

This recipe builds FVWM 1.24r, GoodStuff, FvwmPager and the Helios logo/clock
backdrop. It uses the X11R4 port and helios-unix, including native task
spawning and FIFO channels for module messages. Start the Helios FIFO server
before launching the desktop.

Build with the shared ports tool and an SP2 SDK. Use `--cpu t425` for code
that also runs on T425 systems; the default is T800. Install the stage under
`/helios`, supply `/helios/etc/fvwm/fvwmrc`, and run:

```
fvwm -s -f fvwmrc
```

The relative configuration name selects `$HOME/fvwmrc` if present, then the
system file. It is suitable for GEMDOS filesystems. Modules take their names
from their executable filenames, so an 8.3 installation can rename GoodStuff
to GoodStuf and FvwmPager to FvwmPage. Update both the `Module` commands and
their `*GoodStuf`/`*FvwmPage` configuration prefixes to match.

Local X displays need no `/internet` server. The platform adapter uses native
`select()` when a display connection is a Helios pipe; socket and FIFO sets
use helios-unix polling. Applications launched by FVWM inherit its Helios
session. The session wrapper is available for login managers that need to
create a desktop TFM; a console session can run FVWM directly.

The manager and small helpers start with 32 KiB C heaps, which grow as
needed. Stack sizes remain 64 KiB.

Set `FVWM_BACKDROP=1` before starting FVWM to draw the Helios logo, clock
and background in the manager's event loop. Omit standalone backdrop `Exec`
commands in that configuration. `helios-backdrop` runs the same renderer as a
standalone program. By default it repaints only exposed regions of the solid
background and small logo/clock header, without storing a background pixmap.
This mode suits an Atari X server with limited memory.

For an X server with more memory, use `helios-backdrop -cache`, or set
`FVWM_BACKDROP_CACHE=1` with the integrated renderer. The X server then stores
the header and bottom-right decoration in pixmaps and restores them when
windows move. No drawing requests or exposure events need to travel to the
standalone renderer during a drag. Only a clock change repaints its small
rectangle. The pixmaps occupy about 1.6 MiB on a 1920x1080, 32-bit X server,
with no corresponding pixel buffer on the Transputer. Allocation failure
falls back to the low-memory renderer. Neither mode uses a screen-sized
pixmap.

To test exposure handling against a separate X server:

```
python3 ports/fvwm/tests/run_desktop.py --display :25
```

This moves a test window without moving the pointer. Use a dedicated test
display; the test replaces its background.

Commands with a literal executable path and arguments use native spawning
directly, including quoted and empty arguments. Shell expansion, operators,
builtins and scripts retain shell interpretation. This avoids a waiting shell
behind each ordinary application.

For a desktop owned by an existing console login, set `FVWM_RESTART_WAIT=0`
before starting FVWM. Restart then releases the previous manager task. Leave
it unset when a login manager relies on that task to track the desktop. The
desktop renderer transfers ownership through a root-window property and
closes the previous renderer normally, freeing its graphics resources.
