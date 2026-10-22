Links 2.30 for Helios
============================

Run /helios/local/bin/links URL, or choose Web browser from the FVWM menu.
G opens an address; arrows select links; Enter follows; Left goes back;
Escape opens menus; Q quits. -dump URL prints a formatted page.

The browser and HTML renderer run natively on the Transputer, using the
native Helios libraries and helios-unix.
The supplied terminal profile matches the X11R4 xterm's Latin-1 fonts.
HTML may have other encodings: Links converts text for the terminal.

HTTP uses /internet. HTTPS uses the portable hu_tls provider API, including
TLS-aware read/write/select/close. The default provider offloads cryptography
to OpenSSL in the Linux io-server and validates the chain and original name
against its CA store. Handshake progress is asynchronous. TLS failures are
reported without a plaintext fallback.

DNS uses independent hu-resolve tasks, not fork/vfork. Optional Unix
subprocess features (external viewers and shell commands) are unavailable.
There is no JavaScript, graphical rendering, Transputer-side TLS or compression
library. The browser requests uncompressed responses. Large modern pages
can exceed the available memory or take time to format on a Transputer.
File saves check write/close/rename errors, but cannot promise fsync-style
durability because native Helios has no such primitive.

Per-user settings and bookmarks live in $HOME/.links. System defaults
live in /helios/etc/links.cfg. Cache defaults are reduced for 16 MiB nodes.

Build: python3 ktm-distribution/build_links.py
Build helios-unix first with build_compat.py. The recipe compiles the maintained
links/ sources and support/links adapters, then records source and artifact
hashes. The source bundle, licence and manual accompany the installed port.
