/* A Helios POSIX pid is task-local (usually 42), so kill(pid,0) cannot
 * identify a game started by another shell. Keep the native task name in a
 * sidecar; leave the historical level-zero format intact for recover. */
#include <syslib.h>
#include <codes.h>
#include <nonansi.h>
extern Environ *getenviron(void);

static void helios_owner_name(char *out, const char *lockname)
{
    char *dot;
    strcpy(out,lockname);
    dot=strrchr(out,'.');
    if (dot) *dot=0;
    strcat(out,".own");
}

/* 1 live, 0 definitely absent, -1 unknown (preserve and ask the player). */
static int helios_lock_live(const char *lockname)
{
    char file[BUFSZ+8], task[512], *slash;
    FILE *f;
    Object *parent, *owner;
    word error;
    helios_owner_name(file,lockname);
    if (!(f=fopen(file,"r"))) return -1;
    if (!fgets(task,sizeof(task),f)) {fclose(f); return -1;}
    fclose(f);
    task[strcspn(task,"\r\n")]=0;
    slash=strrchr(task,'/');
    if (!slash || slash==task) return -1;
    *slash=0;
    parent=Locate(NULL,task);
    if (!parent) return -1; /* Processor unavailable is not proof of death. */
    owner=Locate(parent,slash+1);
    error=Result2(parent);
    Close(parent);
    if (owner) {Close(owner); return 1;}
    /* ServLib GetTargetObj reports EG_Name for an absent directory member;
     * the procman directory itself has already been located successfully. */
    return (error & EG_Mask)==EG_Unknown || (error & EG_Mask)==EG_Name ? 0 : -1;
}

static int helios_write_owner(const char *lockname)
{
    char file[BUFSZ+8];
    FILE *f;
    Object *task=getenviron()->Objv[OV_Task];
    int ok;
    if (!task || task==(Object *)MinInt) return 0;
    helios_owner_name(file,lockname);
    if (!(f=fopen(file,"w"))) return 0;
    ok=fprintf(f,"%s\n",task->Name)>0;
    if (fclose(f)) ok=0;
    return ok;
}
