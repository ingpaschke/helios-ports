slrn 1.0.3a / S-Lang 2.3.3 for Helios

Start with your news server (the first run fetches the group list):
  slrn -h news.example.org --create
  slrn -h snews://news.example.org:563 --create
Then omit --create on subsequent runs. Alternatively set NNTPSERVER.
Use -f filename for a separate subscription/read-state file per server.

Arrow keys move, Return enters a group/opens an article, Space scrolls,
q returns or quits, and ? displays help. Use a Helios xterm.
Personal settings go in ~/.slrnrc; subscriptions/read state go in ~/.jnewsrc.
The complete upstream configuration example is installed as sample.slrnrc.
Set username, hostname and realname to your own posting identity. Configure
NNTP authentication with nnrpaccess "server" "username" "password".
For TLS, use nnrpaccess "snews://server" "username" "password" instead.

The native reader includes NNTP, authentication, threading, scoring, S-Lang
macros, article saving and posting through NNTP. The default editor is Stevie;
editor_command can select another editor. Editors run via helios-unix spawn,
without Unix fork semantics. Terminal dimensions use Helios attributes.

TLS (snews://) uses the portable hu_tls provider API for handshake, data and
readiness. The default backend uses the Linux I/O server and checks the
server's hostname and certificate against its trust store; that backend
requires an HTL1 io-server. No plaintext fallback or client
certificates are supported. Shutdown/cancellation uses the TLS adapter.

Local spool/slrnpull, Unix command pipes, printing, sendmail, dynamic S-Lang
modules, iconv, libcanlock and external uudecode helpers are not included.
This is a native Transputer executable; Linux only supplies networking/TLS.
The bundled sources and build recipe record the exact upstream inputs.
