/* Irssi selects the native Helios console-key profile explicitly. */
#include <helios/unix/api.h>
int ktm_terminal_read(int fd,char *out,int size)
{
    static struct hu_terminal_decoder decoder={0,1};
    return hu_terminal_read(fd,&decoder,out,size);
}
