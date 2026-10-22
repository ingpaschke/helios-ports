#include "platform.h"
/* Optional Unix subprocess features fail explicitly until given spawn
 * adapters. In particular, never call Helios's vfork alias as fork. */
int links_no_fork(void) { errno=ENOSYS;return -1; }

int links_terminal_read(int fd,unsigned char *out,int size)
{
    static struct hu_terminal_decoder decoder={0,1};
    return hu_terminal_read(fd,&decoder,(char *)out,size);
}
