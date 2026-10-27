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
commands in that configuration. The renderer uses a solid root background,
redraws its arcs on exposure, and paints the logo/clock in a small header
window; it allocates no screen-sized pixmap. `helios-backdrop` remains
available as a standalone entry point to the same renderer.

Commands with a literal executable path and arguments use native spawning
directly, including quoted and empty arguments. Shell expansion, operators,
builtins and scripts retain shell interpretation. This avoids a waiting shell
behind each ordinary application.

For a desktop owned by an existing console login, set `FVWM_RESTART_WAIT=0`
before starting FVWM. Restart then releases the previous manager task. Leave
it unset when a login manager relies on that task to track the desktop. The
desktop renderer transfers ownership through a root-window property and
closes the previous renderer normally, freeing its graphics resources.
