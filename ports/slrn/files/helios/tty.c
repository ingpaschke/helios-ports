/* Native terminal adapter: preserve Helios size attributes and normalize
 * its CSI/SS3 input before S-Lang decodes the terminal key sequences. */
#include "slinclud.h"
#include "slang.h"
#include "_slang.h"
#include "platform.h"
#include <signal.h>
int SLang_TT_Read_FD=-1,SLang_TT_Baud_Rate=38400;
int (*SLang_getkey_intr_hook)(void)=NULL;
static struct termios saved;
static int initialized,begin,end;
static char keys[64];
static struct hu_terminal_decoder decoder={0,1};
int SLang_init_tty(int abort_char,int no_flow,int opost)
{
    (void)no_flow;(void)opost;
    if(initialized)return 0;
    if(SLang_TT_Read_FD<0)SLang_TT_Read_FD=0;
    if(hu_terminal_raw(SLang_TT_Read_FD,&saved)<0)return -1;
    tcsetpgrp(SLang_TT_Read_FD,getpgrp());
    if(abort_char>=0)SLang_Abort_Char=abort_char;
    initialized=1;return 0;
}
void SLang_reset_tty(void)
{if(initialized)hu_terminal_restore(SLang_TT_Read_FD,&saved);initialized=0;}
void SLtty_set_suspend_state(int mode){(void)mode;}
static void interrupted(int sig)
{(void)sig;SLKeyBoard_Quit=1;if(!SLang_Ignore_User_Abort)SLang_set_error(SL_USER_BREAK);}
int SLang_set_abort_signal(void (*handler)(int))
{return SLsignal_intr(SIGINT,handler?handler:interrupted)==SIG_ERR?-1:0;}
int _pSLsys_input_pending(int ticks)
{
    fd_set set;struct timeval timeout;long ms=ticks<0?-(long)ticks:(long)ticks*100;
    if(begin<end)return 1;
    timeout.tv_sec=ms/1000;timeout.tv_usec=(ms%1000)*1000;
    FD_ZERO(&set);FD_SET(SLang_TT_Read_FD,&set);
    return select(SLang_TT_Read_FD+1,&set,NULL,NULL,&timeout);
}
unsigned int _pSLsys_getkey(void)
{
    int n;
    if(!initialized){n=getchar();return n==EOF?SLANG_GETKEY_ERROR:(unsigned)n;}
    while(begin==end){
        if(SLKeyBoard_Quit)return SLang_Abort_Char;
        n=hu_terminal_read(SLang_TT_Read_FD,&decoder,keys,sizeof(keys));
        if(n<0 && errno==EINTR){
            if(SLang_getkey_intr_hook && SLang_getkey_intr_hook()<0)return SLANG_GETKEY_ERROR;
            continue;
        }
        if(n<=0)return SLANG_GETKEY_ERROR;
        begin=0;end=n;
    }
    return (unsigned char)keys[begin++];
}
