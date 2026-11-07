# nanoprintf

nanoprintf 0.8.1 supplies bounded string formatting for native Helios programs.
The recipe builds the original header implementation into `lib/libnpf.a` and
installs its public header in the build directory. There are no upstream
patches or runtime data files.

`files/helios/config.h` enables field width, precision, floating point, small
and 64-bit integer formats. Binary and `%n` formats are disabled. Applications
use `npf_snprintf` and `npf_vsnprintf`; Nonpareil maps its corresponding libc
calls to those names in its platform header.

The upstream source is dual licensed under 0BSD and the Unlicense. This build
uses 0BSD; its notice is in the prepared source's `LICENSE`.
