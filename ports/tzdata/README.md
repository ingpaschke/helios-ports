# IANA timezone data

This recipe compiles the pinned IANA database into TZif files for Helios.
It needs the host `zic` tool, supplied by the host's timezone utilities.
No Transputer executable is built and no upstream changes are required.

The installation stage contains `/helios/etc/zoneinfo`, including aliases,
and the upstream licence and version under `/helios/local/share/tzdata`.
Select a zone with `TZ=Europe/Berlin` or put its name in
`/helios/etc/timezone`. The resident C library performs the conversion on
Helios; it does not ask Linux to convert individual timestamps.
