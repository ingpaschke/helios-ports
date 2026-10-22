# Native regular expressions

This dependency packages the Henry Spencer regular-expression implementation
already used by Helios text utilities. XView uses it for file chooser filters.
It builds `local/lib/regexp.lib` and `local/include/regexp.h`.

The source is pinned from Helios-NG; `files/helios/error.c` supplies its error
callback. There are no changes to the original expression engine. The
University of Toronto notice is installed in `local/share/regexp/LICENCE`.
