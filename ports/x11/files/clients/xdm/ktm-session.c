/* Native session capabilities for the XDM-authenticated desktop.
 * Keep Helios Object declarations separate from Xt's Object typedef. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslib.h>
#include <nonansi.h>
#include <session.h>
/* The shared X11 build preincludes unistd before syslib, hiding this
 * Helios extension declaration behind unistd's include guard. */
extern Environ *getenviron(void);

static Object *desktop_session, *desktop_tfm;
static Object *saved_session, *saved_tfm;
static RmSessionInfo desktop_info;
static FILE *desktop_log;
static int installed;
static int cleanup_registered;

void KtmCloseNativeSession(void)
{
    if(installed) {
        getenviron()->Objv[OV_Session]=saved_session;
        getenviron()->Objv[OV_TFM]=saved_tfm;
        installed=0;
    }
    if(desktop_session) {
        Delete(desktop_session,NULL);
        Close(desktop_session);desktop_session=NULL;
    }
    if(desktop_tfm) {Close(desktop_tfm);desktop_tfm=NULL;}
    if(desktop_info.TFM) Close(desktop_info.TFM);
    if(desktop_info.CurrentDirectory) Close(desktop_info.CurrentDirectory);
    if(desktop_info.Program) Close(desktop_info.Program);
    memset(&desktop_info,0,sizeof(desktop_info));
    if(desktop_log) {fclose(desktop_log);desktop_log=NULL;}
}

int KtmCreateNativeSession(char *user,char *password,char *home)
{
    char path[512];
    word error=0;
    Environ *env=getenviron();
    if(!cleanup_registered) {atexit(KtmCloseNativeSession);cleanup_registered=1;}
    KtmCloseNativeSession();
    if(!home || strlen(home)+sizeof("/.xsession-errors")>sizeof(path))return 0;
    sprintf(path,"%s/.xsession-errors",home);
    desktop_log=fopen(path,"w+");
    if(!desktop_log) {perror("xdm: session log");return 0;}
    /* The Session Manager explicitly supports a log file for batch sessions;
     * xterm supplies a separate interactive console for each shell. */
    desktop_session=RmCreateSession(user,password,fdstream(fileno(desktop_log)),&error);
    if(!desktop_session)goto failed;
    if(!RmGetCapabilities(desktop_session,&desktop_info,&error))goto failed;
    if(!desktop_info.TFM || !(desktop_tfm=Locate(desktop_info.TFM,"tfm")))goto failed;
    saved_session=env->Objv[OV_Session];saved_tfm=env->Objv[OV_TFM];
    env->Objv[OV_Session]=desktop_session;env->Objv[OV_TFM]=desktop_tfm;
    installed=1;
    fprintf(desktop_log,"XDM desktop session %s, TFM %s\n",desktop_session->Name,desktop_tfm->Name);
    fflush(desktop_log);
    return 1;
failed:
    fprintf(stderr,"xdm: cannot create native desktop session for %s (0x%08lx)\n",user,error);
    KtmCloseNativeSession();
    return 0;
}

char *KtmNativeSessionName(void)
{
    char *p=strrchr(desktop_session->Name,'/');
    return p?p+1:desktop_session->Name;
}

void KtmNativeSessionStreams(void)
{
    if(desktop_log) {
        dup2(fileno(desktop_log),1);
        dup2(fileno(desktop_log),2);
    }
}
