NetHack 3.1.3 for Helios 1.3 / X11R4
===================================

NetHack 3.1.3 runs natively on Transputers with Helios.
It uses the original X11 Athena widget interface, with coloured characters.
It is not a tile-based interface. No tty window port is included.

Installation
------------
Merge both the archive's local and lib directories into your Helios tree.
This creates /helios/local/games/nh313 and installs the X resources under
/helios/lib/x11/app-defa/NetHack. The game directory,
its record/logfile/perm files, and save directory must be writable by the
player. Do not install setuid. Keep existing saves and scores when updating.
Helios must have its original X client libraries, including /helios/lib/x.

Use the T425 build for T400-family CPUs (including T425) and the T800 build
for T800/T805 CPUs. Memory requirements have not been measured. The supplied
emulator test starts with 8 MB; smaller configurations need testing.

Start X with STARTX, then in the Helios shell:

  setenv DISPLAY helios:0
  /helios/local/games/nh313/nethack -u Player

Configure the X server font path to include the 75dpi and NetHack font
directories when using the supplied fonts. The game fonts directory
contains nh10.snf, ibm.snf and fonts.dir for the Helios X11R4 server.
The complete original NetHack.ad is installed in lib/x11/app-defa as NetHack,
including colours, widget styling and key bindings. X finds it there without
XAPPLRESDIR. Do not put this resource beside the nethack program: GEMDOS treats
those two names as identical.
The default map uses nh10 and the supplied nethack.cnf enables its graphics.
For a personal nethack.cnf, merge the shipped OBJECTS and GRAPHICS settings.
The supplied nethack.rc also documents the alternative ibm font mappings.

Select a role from the menu, or specify a suffix such as -u Player-V for a
Valkyrie. Standard movement keys are h j k l y u b n. Use S to save, and
start again with the same name to restore. Use -s to list high scores.
DISPLAY selects the server; this release does not parse X toolkit command
line options such as -display. The 75dpi directory supplies the original
variable interface font; the standard font path supplies fixed and cursor fonts.

NETHACKDIR or the first option -d <directory> overrides the game directory.
Options are read from $HOME/nethack.cnf or the default game's nethack.cnf.
NETHACKOPTIONS can hold comma-separated options or a configuration filename.
If startup repeatedly waits for perm.lck, first ensure no NetHack process is
running in this game directory. Then remove the stale perm.lck (not perm)
and restart. Lock files are closed before deletion for DOS-backed servers.
Names on disk use eight-character basenames, so player names that share the
same first characters (after the numeric user id) may share a save/lock name.
Use distinct short player names. Saves from other ports/builds are not promised
compatible. The recover utility can recover insured levels after a crash;
see the original NetHack recover documentation before using it.

Changes from the original release
--------------------------------
- Native Helios configuration, X11 only, with colour and internal save
  compression; Unix fork-dependent shell, external compression, mail and
  process suspension disabled.
- SDK library declarations, Helios open/access constants, exclusive-file
  locks, .lck lock suffixes and DOS 8.3 save/level names.
- Binary mode for game data streams, preserving offsets and bytes through
  DOS-backed I/O servers.
- Original X resources, compiled nh10/ibm fonts and nh10 graphics mappings;
  fixed-font fallback resources if the application resource file is absent.
- Correct six-digit dates across the year 2000 boundary.
- Animation delays (thrown objects, zaps) sleep for 30 ms instead of
  waiting on an X Toolkit timer, which Helios fires up to a second late.
- Private ILP32 host tools with the Helios minimum structure alignment;
  compile-time checks compare all serialized level record sizes and key
  field offsets with the target compiler.

Source and licence
------------------
NetHack is distributed under the NetHack General Public License, included
as license. Retain it and the original source notices when redistributing.
The accompanying nh313-helios-source.tar.gz contains the modified sources,
build scripts and tests.
The bundled random generator includes software developed by the University
of California, Berkeley. Its original notice is in sys/share/random.c.
The X11 files retain their original MIT/DEC and NetHack notices.
