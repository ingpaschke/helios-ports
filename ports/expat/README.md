# Expat

Expat 2.7.3 supplies the native XML parser used by Nonpareil. The recipe builds
the three upstream library sources without source patches and installs
`lib/libexpat.a` and the public headers in its build directory.

`files/helios/expat_config.h` selects the little endian Helios target, ordinary
character strings and namespaces. Installed calculator definitions do not need
external DTD loading. Helios has no kernel entropy source, so the configuration
uses Expat's documented fallback for its internal hash seed. The compiler's
`__builtin_isnan` supplies the classification absent from the vintage math
header. No runtime server or data files are installed by this library port.

Expat's MIT licence is in the prepared source's `expat/COPYING`.
