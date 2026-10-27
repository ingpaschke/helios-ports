# X11R4 for Helios

The recipe builds the X libraries and native clients from the pinned X11R4
sources. See `port.json` for the patches, targets and installed files.

Local displays (`helios:0`, `local:0`, `unix:0` or `:0`) use native Helios
namespace sockets named `X0`, `X1`, and so on. They need no `/internet`
server. A hostname or IP address selects TCP, for example `192.0.2.1:0`.

Xterm starts its native terminal adapter as `/helios/lib/xttyserv`. The
`xterm-tty` dependency installs that 8.3-compatible name as well as the
existing `xterm-ttyserv` name for other clients.

`XChar2b` has two byte-sized fields, matching the X11 16-bit text protocol.
Rebuild clients that pass `XChar2b` arrays against the supplied headers and
libraries. A prebuilt client compiled with the older four-byte declaration
cannot use those 16-bit text interfaces with the corrected library. The
8-bit text interfaces retain their existing layout.

With `--linkage shared` (the default), `lib/libX*.a` are import libraries for
native resident modules. With `--linkage static`, those paths select ordinary
archives and the X11 resident modules are not installed. Shared builds also retain ordinary archives under `lib/static/`. The shared
modules are `Hx11` (Xlib), `Hxext` (extensions), `Hxt` (Xt and Xmu), `Hxaw`
(Athena widgets), and the `x11-support` dependency's `Hxb` (BSD helpers).
Install their staged `lib/` files together with rebuilt clients. Each processor
shares code among its tasks; each task receives its own writable library data.
The vintage `X` module remains a separate interface for prebuilt clients.

Xt/Xmu share one module with the Athena VendorShell implementation, avoiding
duplicate VendorShell class records and a circular library dependency. The
readable interfaces in `helios/shared/*.json` specify function slots, reserved
capacity and public data sizes. Rebuild clients with these headers and import
libraries; do not mix interfaces from another ABI version.

The terminal is VT-only: the Tektronix parser, graphics backend, options and
menu entries are excluded. It defaults to zero scrollback, a 16 KiB initial
heap which grows as needed, and an unchanged 64 KiB stack. `xterm -sl 64`
requests scrollback explicitly. It still uses `/helios/lib/xttyserv`.

The build produces `tests/x11check` without installing it. Run it while another
Xt client is alive to check private widget state and canonical inherited
function pointers. Run `x11check vt` inside xterm to check that a Tek-mode
escape request leaves the terminal in VT mode.
