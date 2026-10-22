/* Helios PIDs are local to a task's POSIX world. In particular kill(42,0)
 * can signal the reader itself. Use native task names, as in the game ports. */
#include <syslib.h>
#include <codes.h>
extern Environ *getenviron(void);
static void test_lock(char *file)
{
    char name[1024],*slash;
    FILE *f=fopen(file,"r");
    Object *parent,*owner,*self=getenviron()->Objv[OV_Task];
    word error;
    if(!f)return;
    if(!fgets(name,sizeof(name),f)){fclose(f);slrn_exit_error("Invalid lock file: %s",file);return;}
    fclose(f);name[strcspn(name,"\r\n")]=0;
    /* A same-name task after a reboot cannot be this new invocation's lock. */
    if(self && self!=(Object *)MinInt && !strcmp(self->Name,name))return;
    slash=strrchr(name,'/');
    if(!slash || slash==name){slrn_exit_error("Old/invalid lock: remove %s if no reader is running",file);return;}
    *slash=0;parent=Locate(NULL,name);
    if(!parent){slrn_exit_error("Cannot check lock owner; processor unavailable: %s",name);return;}
    owner=Locate(parent,slash+1);error=Result2(parent);Close(parent);
    if(owner){Close(owner);slrn_exit_error("Another slrn task owns %s",file);return;}
    if((error&EG_Mask)!=EG_Unknown && (error&EG_Mask)!=EG_Name)
        slrn_exit_error("Cannot determine whether lock is stale: %s",file);
}
static int make_lock(char *file)
{
    FILE *f;int ok;
    Object *self=getenviron()->Objv[OV_Task];
    if(!self || self==(Object *)MinInt)return -1;
    f=fopen(file,"w");if(!f)return -1;
    ok=fprintf(f,"%s\n",self->Name)>0;
    if(fclose(f))ok=0;
    return ok?0:-1;
}
