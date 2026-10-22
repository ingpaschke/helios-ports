# Licences

The shared `ports.py`, `tools/`, build-tool tests and recipe metadata are
licensed under [MIT](LICENSE.tools). This does not relicense the applications,
assets or modifications carried in their patches.

Component notices are retained in the reconstructed source trees. Run
`python3 ports.py prepare <name>` to inspect them under `work/sources/<name>`.
The following paths are relative to each component's prepared tree; also
consult individual source and asset notices.

| Component | Notices |
| --- | --- |
| X11R4 | `COPYRIGHTS` and individual source notices |
| FVWM | `fvwm/fvwm.c`; modules include their own notices, such as `modules/FvwmIconBox/COPYRIGHT` |
| Irssi | `COPYING` |
| GLib | `COPYING`, `gmodule/COPYING` |
| Links | `COPYING` |
| slrn | `COPYING` |
| S-Lang | `COPYING` |
| Xmahjongg | `COPYING`, `README`; X11 headers come from the shared X11 port |
| ncurses | `COPYING`; new native key adapter and regression probes carry MIT notices |
| OLWM, OLGX | Sun `LEGAL_NOTICE`, GPLv3 for later upstream patches (`COPYING.GPL3`); new native adapters carry MIT notices |
| Frotz | `COPYING` |
| NetHack | `dat/license`, `sys/share/random.c` |
| Telnet | Berkeley notices in `telnet.c` and other sources |
| xterm-tty | Parsytec notices in `tty.c` and other sources |

New port adapters are supplied under `ports/<name>/files/`, at their paths
relative to the upstream source tree. Changes to existing files are in patches.
Keep the relevant notices when redistributing modified sources and data.
