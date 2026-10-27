# Native X11 support for Helios

This recipe rebuilds the BSD helpers used by the X11R4 libraries as the native
resident module `Hxb`. It uses pinned Helios-NG sources and the same 32-bit
Helios interfaces as the SDK's static BSD library. It requires an SP2 SDK,
llvm-t800 and the SDK assembler/AMPP tools.

Install `stage/lib/Hxb` with the X11R4 resident modules. The ports builder
follows the import library's dependency metadata when linking clients. Code
is shared on each processor; globals such as the error and signal tables
remain private to each task. Other software can continue using `bsd.lib`.

The runtime sources retain their Helios-NG licence; see `COPYING`. The recipe
and shared build tooling use the repository's MIT tooling licence.
