/* Platform hooks for the X11-only Helios port. */
#define NEED_VARARGS
#include "hack.h"

/* X11 owns input; pager.c's terminal interrupt hooks need no tty changes. */
void intron(void) {}
void introff(void) {}

void
error VA_DECL(const char *,s)
    VA_START(s);
    VA_INIT(s, const char *);
    Vfprintf(stderr, s, VA_ARGS);
    (void) fputc('\n', stderr);
    VA_END();
    exit(1);
}
