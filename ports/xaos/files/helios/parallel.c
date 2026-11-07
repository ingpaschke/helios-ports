/* SPDX-License-Identifier: GPL-2.0-or-later
 * Distributed XaoS pixel calculations over native Helios buffered channels.
 * Xlib and the incremental image/reallocation tables stay with the UI task.
 * Each child has its own XaoS formula state, coordinates and palette. */
#include <config.h>
#include <filter.h>
#include <fractal.h>
#include <plane.h>
#include "../src/engine/calculate.h"
#include "parallel.h"
#include <syslib.h>
#include <rmlib.h>
#include <helios/unix/channel.h>
#include <helios/unix/spawn.h>
#include <helios/unix/poll.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>

#define HX_MAGIC 0x58414f53U
#define HX_VERSION 2
#define HX_WORKERS 31
#define HX_BATCH 512
#define HX_LOCAL_BATCH 64
#define HX_DIM 4096
#define HX_COLORS 65536
#define HX_FRAME 1
#define HX_JOB 2
#define HX_STOP 3

/* Pointer-free protocol, used by identical Helios executables on the nodes.
 * Integers are 32-bit and number_t is IEEE double in this build. */
struct hx_header { unsigned magic,version,command,epoch; };
struct hx_state {
    struct hx_header header;
    int width,height,bpp,formula,palette_size,palette_type,palette_start,palette_end;
    int palette_version,palette_update;
    number_t pre,pim,bre,bim,angle,bailout;
    unsigned maxiter;
    int periodicity,coloringmode,incoloringmode,intcolor,outtcolor;
    int mandelbrot,plane,range,slowmode;
    float windowwidth,windowheight;
    vinfo view;
    union paletteinfo palette_info;
};
struct hx_job { struct hx_header header; unsigned sequence,first,count; };
struct hx_reply { unsigned magic,epoch,sequence,count,pixels[HX_BATCH]; };
struct hx_worker {
    int input,output,alive,busy;
    pid_t pid;
    RmProcessor processor;
    char name[256];
    struct hx_job pending;
    unsigned long batches,pixels;
    long dispatched;
    int palette_valid,palette_version;
};
static struct hx_worker workers[HX_WORKERS];
static int nworkers,started,requested=0;
static unsigned epoch,sequence;
static char program[1024],local_name[256];
static unsigned long local_batches,local_pixels;
static int check_failure,failure_injected,profile,benchmark,preview_ready;
int hx_preview_ready(void){return preview_ready;}
/* Index only missing pixels. Clean rows use the changed-column list; dirty
 * rows contain every column. Jobs carry a range of ordinals in this view,
 * rather than sending a four-byte coordinate for every pixel. */
static struct {
    unsigned columns[HX_DIM],rows[HX_DIM+1],order[HX_DIM],ncolumns,total,height,width;
    unsigned remaining_rows[HX_DIM],remaining_columns[HX_DIM];
    unsigned char dirty[HX_DIM],changed[HX_DIM];
    unsigned char *output_dx,*output_dy;
} scan;
static void prepare_scan(const unsigned char *dx,const unsigned char *dy)
{
    unsigned x,y,k,bits=0,power=1,n=0;
    scan.width=cimage.width;scan.height=cimage.height;scan.ncolumns=0;
    scan.output_dx=scan.output_dy=NULL;preview_ready=0;
    for(x=0;x<scan.width;x++) {
        scan.changed[x]=dx[x]!=0;scan.remaining_columns[x]=dx[x]?scan.height:0;
        if(dx[x])scan.columns[scan.ncolumns++]=x;
    }
    /* Spread completed rows over the view so a time-limited preview can
     * interpolate throughout the image instead of filling it top to bottom. */
    while(power<scan.height){power*=2;bits++;}
    for(k=0;k<power;k++) {
        unsigned value=k,reversed=0,i;
        for(i=0;i<bits;i++){reversed=(reversed<<1)|(value&1);value>>=1;}
        if(reversed<scan.height)scan.order[n++]=reversed;
    }
    scan.rows[0]=0;
    for(k=0;k<scan.height;k++) {
        y=scan.order[k];scan.dirty[y]=dy[y]!=0;
        scan.remaining_rows[y]=scan.dirty[y]?scan.width:scan.ncolumns;
        scan.rows[k+1]=scan.rows[k]+scan.remaining_rows[y];
    }
    scan.total=scan.rows[scan.height];
}
static unsigned first_row(unsigned first)
{
    unsigned low=0,high=scan.height;
    while(low<high) {
        unsigned mid=low+(high-low)/2;
        if(scan.rows[mid+1]<=first)low=mid+1;else high=mid;
    }
    return low;
}

static long milliseconds(void)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return (long)((unsigned long)tv.tv_sec*1000UL+(unsigned)tv.tv_usec/1000);
}
int hx_profile_enabled(void){return profile;}
long hx_milliseconds(void){return milliseconds();}
static int transfer(int fd,void *buffer,unsigned bytes,int writing)
{
    char *p=buffer;
    while(bytes) {
        int n=writing?write(fd,p,bytes):read(fd,p,bytes);
        if(n<0&&errno==EINTR)continue;
        if(n<=0)return 0;
        p+=n;bytes-=n;
    }
    return 1;
}
#define SEND(fd,p,n) transfer(fd,(void *)(p),(n),1)
#define RECV(fd,p,n) transfer(fd,(p),(n),0)
static int ready(int fd,int timeout)
{
    struct hu_pollfd p={fd,HU_POLLIN,0};
    int result;
    do result=hu_poll(&p,1,timeout);while(result<0&&errno==EINTR);
    return result>0&&(p.revents&HU_POLLIN);
}
static void retire(struct hx_worker *w)
{
    int status,i;
    if(w->input>=0)close(w->input);
    if(w->output>=0)close(w->output);
    w->input=w->output=-1;w->alive=w->busy=0;
    if(w->pid>0) {
        /* Channel EOF normally ends a worker. A failed node must not hold
         * up desktop shutdown indefinitely. Only our own child is signalled. */
        for(i=0;i<20;i++) {
            if(waitpid(w->pid,&status,WNOHANG)==w->pid)break;
            Delay(10000);
        }
        if(i==20){kill(w->pid,SIGKILL);waitpid(w->pid,&status,WNOHANG);}
        w->pid=0;
    }
    if(w->processor){RmReleaseProcessor(w->processor);RmFreeProcessor(w->processor);w->processor=NULL;}
}
static void shutdown_pool(void)
{
    int i;
    struct hx_header h={HX_MAGIC,HX_VERSION,HX_STOP,0};
    for(i=0;i<nworkers;i++) {
        struct hx_worker *w=workers+i;
        if(w->alive&&!w->busy)SEND(w->input,&h,sizeof(h));
        retire(w);
    }
}
static int start_worker(RmProcessor p,...)
{
    struct hx_worker *w;
    struct hu_spawn_actions actions;
    struct hu_spawn_attr attr={HU_SPAWN_PROCESSOR,NULL,NULL,3};
    int in[2]={-1,-1},out[2]={-1,-1},error;
    unsigned hello[3];
    char path[256];
    char *args[]={program,"--helios-worker",NULL};
    RmProcessor obtained;
    if(nworkers>=HX_WORKERS||(requested>0&&nworkers>=requested-1))return 0;
    if((RmGetProcessorPurpose(p)&RmP_Mask)!=RmP_Helios ||
       (RmGetProcessorPurpose(p)&RmP_System) ||
       !(RmGetProcessorState(p)&RmS_Running) ||
       RmGetProcessorType(p)!=RmT_T800)return 0;
    RmBuildProcessorName(path,p);
    if(!strcmp(path,local_name))return 0;
    obtained=RmObtainProcessor(p);
    if(!obtained)return 0;
    w=workers+nworkers;memset(w,0,sizeof(*w));
    w->input=w->output=-1;w->processor=obtained;
    strncpy(w->name,path,sizeof(w->name)-1);
    if(hu_channel(in)<0||hu_channel(out)<0)goto fail;
    hu_spawn_actions_init(&actions);
    error=hu_spawn_adddup2(&actions,in[0],0);
    if(!error)error=hu_spawn_adddup2(&actions,out[1],1);
    attr.processor=w->name;
    if(!error)error=hu_spawnp(&w->pid,program,&actions,&attr,args,NULL);
    hu_spawn_actions_destroy(&actions);
    close(in[0]);in[0]=-1;close(out[1]);out[1]=-1;
    if(error){fprintf(stderr,"XaoS: %s worker startup: %s\n",path,strerror(error));goto fail;}
    w->input=in[1];in[1]=-1;w->output=out[0];out[0]=-1;
    if(!ready(w->output,10000)||!RECV(w->output,hello,sizeof(hello))||
       hello[0]!=HX_MAGIC||hello[1]!=HX_VERSION||hello[2]!=sizeof(number_t))goto fail;
    w->alive=1;nworkers++;
    fprintf(stderr,"XaoS: render worker on %s\n",w->name);
    return 0;
fail:
    if(in[0]>=0)close(in[0]);if(in[1]>=0)close(in[1]);
    if(out[0]>=0)close(out[0]);if(out[1]>=0)close(out[1]);
    retire(w);return 0;
}
static void start_pool(void)
{
    RmNetwork network;
    if(started)return;
    started=1;MachineName(local_name);atexit(shutdown_pool);
    if(requested==1)return;
    network=RmGetNetwork();
    if(network){RmApplyProcessors(network,start_worker);RmFreeNetwork(network);}
    else fprintf(stderr,"XaoS: network discovery: %s; rendering locally\n",RmMapErrorToString(RmErrno));
    fprintf(stderr,"XaoS: %d render processors (%s plus %d workers)\n",1+nworkers,local_name,nworkers);
}
int hx_parallel_enabled(void){return requested!=1;}
int hx_processor_count(void)
{
    int i,n=1;for(i=0;i<nworkers;i++)if(workers[i].alive)n++;return n;
}

static void pack_state(struct hx_state *s)
{
    memset(s,0,sizeof(*s));
    s->header.magic=HX_MAGIC;s->header.version=HX_VERSION;
    s->header.command=HX_FRAME;s->header.epoch=epoch;
    s->width=cimage.width;s->height=cimage.height;s->bpp=cimage.bytesperpixel;
    s->formula=(int)(cfractalc.currentformula-formulas);
    s->palette_size=cpalette.size;s->palette_type=cpalette.type;
    s->palette_version=cpalette.version;
    s->palette_start=cpalette.start;s->palette_end=cpalette.end;
#define FIELD(n) s->n=cfractalc.n
    FIELD(pre);FIELD(pim);FIELD(bre);FIELD(bim);FIELD(angle);FIELD(bailout);
    FIELD(maxiter);FIELD(periodicity);FIELD(coloringmode);FIELD(incoloringmode);
    FIELD(intcolor);FIELD(outtcolor);FIELD(mandelbrot);FIELD(plane);FIELD(range);
    FIELD(slowmode);FIELD(windowwidth);FIELD(windowheight);
#undef FIELD
    s->view=cfractalc.s;s->palette_info=cpalette.info;
}
static int install_state(const struct hx_state *s,unsigned *pixels)
{
    struct fractal_context fc;
    struct image image;
    struct palette palette;
    if(s->width<1||s->width>HX_DIM||s->height<1||s->height>HX_DIM||
       s->formula<0||s->formula>=nformulas||s->palette_size<2||
       s->palette_size>HX_COLORS||s->bpp<1||s->bpp>4)return 0;
    memset(&fc,0,sizeof(fc));memset(&image,0,sizeof(image));memset(&palette,0,sizeof(palette));
#define FIELD(n) fc.n=s->n
    FIELD(pre);FIELD(pim);FIELD(bre);FIELD(bim);FIELD(angle);FIELD(bailout);
    FIELD(maxiter);FIELD(periodicity);FIELD(coloringmode);FIELD(incoloringmode);
    FIELD(intcolor);FIELD(outtcolor);FIELD(mandelbrot);FIELD(plane);FIELD(range);
    FIELD(slowmode);FIELD(windowwidth);FIELD(windowheight);
#undef FIELD
    fc.s=s->view;fc.currentformula=formulas+s->formula;fc.palette=&palette;
    palette.pixels=pixels;palette.size=palette.maxentries=s->palette_size;
    palette.type=s->palette_type;palette.start=s->palette_start;palette.end=s->palette_end;
    palette.info=s->palette_info;palette.version=s->palette_version;
    image.width=s->width;image.height=s->height;image.bytesperpixel=s->bpp;image.palette=&palette;
    set_fractalc(&fc,&image);
    cfractalc.palette = &cpalette; cimage.palette = &cpalette;
    /* set_fractalc copies the structures, so only the palette pixel array
     * needs to remain allocated. No pointer in the wire message is used. */
    return 1;
}
static void calculate_job(const struct hx_job *job,struct hx_reply *reply,
                          const number_t *xp,const number_t *yp)
{
    unsigned i,row=first_row(job->first),y=scan.order[row],column=job->first-scan.rows[row];
    reply->magic=HX_MAGIC;reply->epoch=job->header.epoch;
    reply->sequence=job->sequence;reply->count=job->count;
    for(i=0;i<job->count;i++) {
        unsigned x=scan.dirty[y]?column:scan.columns[column];
        reply->pixels[i]=calculate(xp[x],yp[y],cfractalc.periodicity);
        if(++column==scan.rows[row+1]-scan.rows[row]) {
            column=0;do row++;while(row<scan.height&&scan.rows[row+1]==scan.rows[row]);
            if(row<scan.height)y=scan.order[row];
        }
    }
}
static void put_reply(const struct hx_job *job,const struct hx_reply *reply)
{
    unsigned i,row=first_row(job->first),y=scan.order[row],column=job->first-scan.rows[row];
    for(i=0;i<job->count;i++) {
        unsigned x=scan.dirty[y]?column:scan.columns[column],value=reply->pixels[i];
        unsigned char *p=cimage.currlines[y]+x*cimage.bytesperpixel;
        switch(cimage.bytesperpixel) {
        case 1:*p=(unsigned char)value;break;
        case 2:{unsigned short v=(unsigned short)value;memcpy(p,&v,2);break;}
        case 3:{unsigned short v=(unsigned short)value;memcpy(p,&v,2);p[2]=value>>16;break;}
        case 4:memcpy(p,&value,4);break;
        }
        if(scan.output_dy&&!--scan.remaining_rows[y]){scan.output_dy[y]=0;preview_ready=1;}
        if(scan.output_dx&&scan.changed[x]&&!--scan.remaining_columns[x])scan.output_dx[x]=0;
        if(++column==scan.rows[row+1]-scan.rows[row]) {
            column=0;do row++;while(row<scan.height&&scan.rows[row+1]==scan.rows[row]);
            if(row<scan.height)y=scan.order[row];
        }
    }
}
static int make_job(struct hx_job *job,unsigned *position,unsigned limit)
{
    unsigned remaining=scan.total-*position;
    if(!remaining)return 0;
    job->header.magic=HX_MAGIC;job->header.version=HX_VERSION;
    job->header.command=HX_JOB;job->header.epoch=epoch;
    job->sequence=++sequence;job->first=*position;
    job->count=remaining<limit?remaining:limit;
    *position+=job->count;
    return 1;
}
static void local_job(const struct hx_job *job,const number_t *xp,const number_t *yp)
{
    struct hx_reply reply;
    calculate_job(job,&reply,xp,yp);put_reply(job,&reply);
    local_batches++;local_pixels+=job->count;
}
int hx_render(const number_t *xp,const number_t *yp,unsigned char *dx,
              unsigned char *dy,int (*idle)(void))
{
    struct hx_state state;
    struct hx_job job;
    unsigned position=0;
    int i,active,missing=0,interrupted=0;
    long begin=milliseconds(),state_end,local_time=0,poll_time=0;
    unsigned long before_local=local_pixels,before_remote=0,after_remote=0;
    if(profile)for(i=0;i<nworkers;i++)before_remote+=workers[i].pixels;
    if(cimage.width<1||cimage.height<1||cimage.width>HX_DIM||cimage.height>HX_DIM||
       cimage.bytesperpixel<1||cimage.bytesperpixel>4||cpalette.size<2||cpalette.size>HX_COLORS)return 0;
    for(i=0;i<cimage.width;i++)missing|=dx[i];
    for(i=0;i<cimage.height;i++)missing|=dy[i];
    if(!missing)return 1;
    prepare_scan(dx,dy);scan.output_dx=dx;scan.output_dy=dy;
    start_pool();epoch++;pack_state(&state);
    for(i=0;i<nworkers;i++) {
        struct hx_worker *w=workers+i;
        if(!w->alive)continue;
        state.palette_update=!w->palette_valid||w->palette_version!=cpalette.version;
        if(!SEND(w->input,&state,sizeof(state))||
           (state.palette_update&&!SEND(w->input,cpalette.pixels,cpalette.size*sizeof(unsigned)))||
           !SEND(w->input,xp,cimage.width*sizeof(number_t))||
           !SEND(w->input,yp,cimage.height*sizeof(number_t))||
           !SEND(w->input,dx,cimage.width)||!SEND(w->input,dy,cimage.height)){retire(w);continue;}
        w->palette_valid=1;w->palette_version=cpalette.version;
    }
    state_end=milliseconds();
    do {
        if(idle&&idle()&&preview_ready)interrupted=1;
        struct hu_pollfd polls[HX_WORKERS];int map[HX_WORKERS],n=0;
        active=0;
        for(i=0;i<nworkers;i++) {
            struct hx_worker *w=workers+i;
            if(!w->alive)continue;
            if(!interrupted&&!w->busy&&make_job(&w->pending,&position,HX_BATCH)) {
                w->busy=1;w->dispatched=milliseconds();
                if(!SEND(w->input,&w->pending,sizeof(w->pending))) {
                    local_job(&w->pending,xp,yp);retire(w);continue;
                }
                if(check_failure){kill(w->pid,SIGKILL);check_failure=0;failure_injected=1;}
            }
            if(w->busy){polls[n].fd=w->output;polls[n].events=HU_POLLIN;polls[n].revents=0;map[n++]=i;active++;}
        }
        if(!interrupted&&make_job(&job,&position,HX_LOCAL_BATCH)){
            long t=profile?milliseconds():0;local_job(&job,xp,yp);
            if(profile)local_time+=(unsigned long)(milliseconds()-t);
        }
        if(n) {
            int result;long t=profile?milliseconds():0;
            do result=hu_poll(polls,n,(interrupted||position>=scan.total)?20:0);while(result<0&&errno==EINTR);
            if(profile)poll_time+=(unsigned long)(milliseconds()-t);
            for(i=0;i<n;i++) {
                struct hx_worker *w=workers+map[i];
                if(!polls[i].revents&&result>=0)continue;
                struct hx_reply reply;
                if((polls[i].revents&HU_POLLIN)&&RECV(w->output,&reply,sizeof(reply)-sizeof(reply.pixels)+w->pending.count*sizeof(unsigned))&&
                   reply.magic==HX_MAGIC&&reply.epoch==epoch&&
                   reply.sequence==w->pending.sequence&&reply.count==w->pending.count) {
                    put_reply(&w->pending,&reply);w->batches++;w->pixels+=reply.count;
                    w->busy=0;
                } else {
                    fprintf(stderr,"XaoS: worker %s lost; recalculating its batch locally\n",w->name);
                    local_job(&w->pending,xp,yp);retire(w);
                }
            }
            /* Deadlines belong to individual jobs: progress on another
             * processor must not hide a stalled worker indefinitely. */
            for(i=0;i<nworkers;i++)if(workers[i].alive&&workers[i].busy&&
                (unsigned long)(milliseconds()-workers[i].dispatched)>30000UL) {
                fprintf(stderr,"XaoS: worker %s timed out\n",workers[i].name);
                local_job(&workers[i].pending,xp,yp);retire(workers+i);
            }
        }
    } while((!interrupted&&position<scan.total)||active);
    /* Jobs finish rows before they finish columns. Keeping a column dirty
     * would select already completed rows again on the next preview pass.
     * All replies are drained now: retain completed rows and describe the
     * remaining work as whole rows. Partially completed rows may be repeated. */
    if (interrupted && position < scan.total) {
        memset(dx, 0, cimage.width);
        for (i = 0; i < cimage.height; i++) {
            dy[i] = scan.remaining_rows[i] != 0;
        }
    }
    if(profile) {
        for(i=0;i<nworkers;i++)after_remote+=workers[i].pixels;
        fprintf(stderr,"XaoS profile render: %dx%d, %d processors, %lu ms total, %lu ms setup, %ld ms local calculation, %ld ms poll, %lu local + %lu remote pixels\n",
            cimage.width,cimage.height,hx_processor_count(),
            (unsigned long)(milliseconds()-begin),(unsigned long)(state_end-begin),
            local_time,poll_time,local_pixels-before_local,after_remote-before_remote);
    }
    scan.output_dx=scan.output_dy=NULL;
    return 1;
}
static int reserve(void **buffer,unsigned *capacity,unsigned need,unsigned itemsize)
{
    void *p;
    if(need<=*capacity)return 1;
    p=realloc(*buffer,need*itemsize);if(!p)return 0;
    *buffer=p;*capacity=need;return 1;
}
static int worker_main(void)
{
    unsigned hello[3]={HX_MAGIC,HX_VERSION,sizeof(number_t)};
    struct hx_state state;
    unsigned *pixels=NULL;
    number_t *xp=NULL,*yp=NULL;
    unsigned char dx[HX_DIM],dy[HX_DIM];
    int status=0;unsigned pc=0,xc=0,yc=0;
    signal(SIGPIPE,SIG_IGN);
    if(!SEND(1,hello,sizeof(hello)))return 1;
    memset(&state,0,sizeof(state));
    for(;;) {
        struct hx_header h;
        if(!RECV(0,&h,sizeof(h)))break;
        if(h.magic!=HX_MAGIC||h.version!=HX_VERSION){status=1;break;}
        if(h.command==HX_STOP)break;
        if(h.command==HX_FRAME) {
            state.header=h;
            if(!RECV(0,(char *)&state+sizeof(h),sizeof(state)-sizeof(h)))break;
            if(state.width<1||state.width>HX_DIM||state.height<1||state.height>HX_DIM||
               state.palette_size<2||state.palette_size>HX_COLORS){status=1;break;}
            if(!reserve((void **)&pixels,&pc,state.palette_size,sizeof(unsigned))||
               !reserve((void **)&xp,&xc,state.width,sizeof(number_t))||
               !reserve((void **)&yp,&yc,state.height,sizeof(number_t))){status=1;break;}
            if((state.palette_update&&!RECV(0,pixels,state.palette_size*sizeof(unsigned)))||
               !RECV(0,xp,state.width*sizeof(number_t))||
               !RECV(0,yp,state.height*sizeof(number_t))||
               !RECV(0,dx,state.width)||!RECV(0,dy,state.height)||
               !install_state(&state,pixels)){status=1;break;}
            prepare_scan(dx,dy);
        } else if(h.command==HX_JOB) {
            struct hx_job job;struct hx_reply reply;
            job.header=h;
            if(!RECV(0,(char *)&job+sizeof(h),sizeof(job)-sizeof(h)))break;
            if(!pixels||h.epoch!=state.header.epoch||!job.count||job.count>HX_BATCH||
               job.first>=scan.total||job.count>scan.total-job.first){status=1;break;}
            calculate_job(&job,&reply,xp,yp);
            if(!SEND(1,&reply,sizeof(reply)-sizeof(reply.pixels)+job.count*sizeof(unsigned)))break;
        } else {status=1;break;}
    }
    free(pixels);free(xp);free(yp);return status;
}

static int stop_preview(void){return 1;}

static unsigned pending_pixels(const unsigned char *columns,
                               const unsigned char *rows)
{
    unsigned changed_columns = 0;
    unsigned pending = 0;
    int x, y;

    for (x = 0; x < cimage.width; x++) {
        changed_columns += columns[x] != 0;
    }
    for (y = 0; y < cimage.height; y++) {
        pending += rows[y] ? cimage.width : changed_columns;
    }
    return pending;
}

static int refine_preview(const number_t *x_positions,
                          const number_t *y_positions,
                          unsigned char *columns, unsigned char *rows)
{
    unsigned pending = pending_pixels(columns, rows);

    while (pending != 0) {
        unsigned previous = pending;

        if (!hx_render(x_positions, y_positions, columns, rows, stop_preview)) {
            return 0;
        }
        pending = pending_pixels(columns, rows);
        if (pending >= previous) {
            fprintf(stderr, "FAIL: preview refinement repeated %u pending pixels\n",
                    pending);
            return 0;
        }
    }
    return 1;
}

static int selfcheck(void);
int hx_entry(int *argc,char **argv)
{
    int i,out=1,check=0;
    Object *object;
    Environ *environment;
    if(*argc>1&&!strcmp(argv[1],"--helios-worker"))return worker_main()+1;
    /* The shell can pass just "xaos" in argv[0]. Use the actual source
     * object supplied at task startup, so children run this same executable
     * even when the current directory or PATH contains a different file. */
    environment=getenviron();
    object=environment&&environment->Objv?environment->Objv[OV_Source]:NULL;
    if(object&&object!=(Object *)MinInt)
        strncpy(program,object->Name,sizeof(program)-1);
    else {
        object=Locate(cdobj(),argv[0]);
        if(object){strncpy(program,object->Name,sizeof(program)-1);Close(object);}
        else strncpy(program,argv[0],sizeof(program)-1);
    }
    for(i=1;i<*argc;i++) {
        if(!strcmp(argv[i],"-help"))
            puts("Helios: -processors all|1..32 selects render processors (default all).");
        if(!strcmp(argv[i],"-processors")) {
            char *end;
            if(++i>=*argc){fprintf(stderr,"XaoS: -processors needs all or 1..32\n");return 3;}
            if(!strcmp(argv[i],"all"))requested=0;
            else {long value=strtol(argv[i],&end,10);if(*end||value<1||value>32){fprintf(stderr,"XaoS: invalid processor count\n");return 3;}requested=value;}
        } else if(!strcmp(argv[i],"-helios-profile"))profile=1;
        else if(!strcmp(argv[i],"--helios-benchmark")){check=1;benchmark=1;profile=1;}
        else if(!strcmp(argv[i],"--helios-check"))check=1;
        else if(!strcmp(argv[i],"--helios-check-failure")){check=1;check_failure=1;}
        else argv[out++]=argv[i];
    }
    argv[out]=NULL;*argc=out;
    signal(SIGPIPE,SIG_IGN);
    if(check)return selfcheck()+1;
    return 0;
}
static int selfcheck(void)
{
    int width=benchmark?640:96,height=benchmark?480:64;
    unsigned color[256];
    unsigned char **line=malloc(height*sizeof(*line)),*data=malloc(width*height*4);
    unsigned char *dx=malloc(width),*dy=malloc(height);
    number_t *xp=malloc(width*sizeof(*xp)),*yp=malloc(height*sizeof(*yp));
    struct image image;struct palette palette;fractal_context *fc;
    unsigned *expected=malloc(width*height*sizeof(unsigned));
    int i,x,y,test,errors=0,bpp,pass,want_failure=check_failure;
    long begin;
    if(!expected||!line||!data||!dx||!dy||!xp||!yp)return 1;
    if(check_failure&&requested==1){free(expected);fputs("Failure check needs a remote worker\n",stderr);return 1;}
    memset(&image,0,sizeof(image));memset(&palette,0,sizeof(palette));
    for(i=0;i<256;i++)color[i]=i?((i*37U)&255U)|(((i*17U)&255U)<<8)|(((i*53U)&255U)<<16):0;
    palette.type=TRUECOLOR;palette.size=palette.maxentries=256;palette.pixels=color;
    palette.info.truec.rmask=0xff0000;palette.info.truec.gmask=0xff00;palette.info.truec.bmask=0xff;
    image.width=width;image.height=height;image.bytesperpixel=4;image.palette=&palette;image.currlines=line;
    for(y=0;y<height;y++)line[y]=data+y*width*4;
    fc=make_fractalc(0,width,height);if(!fc){free(expected);return 1;}
    for(bpp=benchmark?4:1;bpp<=4;bpp++) {
      unsigned mask=bpp==4?~0U:((1U<<(bpp*8))-1);
      image.bytesperpixel=bpp;
      for(y=0;y<height;y++)line[y]=data+y*width*bpp;
      for(test=0;test<4;test++) {
        set_formula(fc,test<3?test:0);fc->maxiter=200;fc->coloringmode=test==2?1:0;
        fc->mandelbrot=test!=3;fc->angle=0;
        if(test==3){fc->pre=-.7;fc->pim=.27015;fc->angle=17;}
        /* Change the same palette allocation to check worker cache invalidation. */
        for(i=0;i<256;i++)color[i]^=0x123456;
        palette.version++;
        set_fractalc(fc,&image);
        for(x=0;x<width;x++){xp[x]=cfractalc.rs.nc+(cfractalc.rs.mc-cfractalc.rs.nc)*x/width;dx[x]=1;}
        for(y=0;y<height;y++){yp[y]=cfractalc.rs.ni+(cfractalc.rs.mi-cfractalc.rs.ni)*y/height;dy[y]=1;}
        begin=milliseconds();
        for(y=0;y<height;y++)for(x=0;x<width;x++)expected[y*width+x]=calculate(xp[x],yp[y],cfractalc.periodicity)&mask;
        printf("Reference formula %d bpp %d: %ld ms\n",test,bpp,(unsigned long)(milliseconds()-begin));fflush(stdout);
        memset(data,0,width*height*4);begin=milliseconds();
        if(!hx_render(xp,yp,dx,dy,NULL)){errors++;break;}
        for(y=0;y<height;y++)for(x=0;x<width;x++) {
            unsigned got=0;memcpy(&got,line[y]+x*bpp,bpp);
            if(got!=expected[y*width+x])errors++;
        }
        printf("Distributed formula %d bpp %d: %ld ms; mismatched pixels=%d\n",test,bpp,(unsigned long)(milliseconds()-begin),errors);fflush(stdout);
        /* Sparse cross, rows only with empty leading rows, columns only,
         * and no changes; all pixels outside each selection must survive. */
        for(pass=0;pass<(benchmark?1:4);pass++) {
            memset(dx,0,width);memset(dy,0,height);
            if(pass==0){dx[13]=1;dy[29]=1;}
            if(pass==1)dy[height-1]=1;
            if(pass==2)dx[width-1]=1;
            for(y=0;y<height;y++)for(x=0;x<width;x++)if(dx[x]||dy[y])memset(line[y]+x*bpp,0xde,bpp);
            if (!hx_render(xp, yp, dx, dy, benchmark ? NULL : stop_preview) ||
                !refine_preview(xp, yp, dx, dy)) {
                errors++;
                break;
            }
            for(y=0;y<height;y++)for(x=0;x<width;x++) {
                unsigned got=0;memcpy(&got,line[y]+x*bpp,bpp);
                if(got!=expected[y*width+x])errors++;
            }
        }
        if(!benchmark) {
            int missing=0;
            memset(dx,1,width);memset(dy,1,height);memset(data,0xde,width*height*4);
            if(!hx_render(xp,yp,dx,dy,stop_preview)){errors++;break;}
            for(y=0;y<height;y++)missing+=dy[y]!=0;
            if(!missing){fputs("FAIL: preview ignored its interrupt\n",stderr);errors++;}
            if(!refine_preview(xp,yp,dx,dy)){errors++;break;}
            for(y=0;y<height;y++)for(x=0;x<width;x++) {
                unsigned got=0;memcpy(&got,line[y]+x*bpp,bpp);
                if(got!=expected[y*width+x])errors++;
            }
        }
      }
    }
    if(requested!=1&&!nworkers){errors++;fputs("FAIL: no remote worker started for the distributed check\n",stderr);}
    if(want_failure&&!failure_injected){errors++;fputs("FAIL: no worker available for the failure check\n",stderr);}
    printf("Local %s: %lu batches, %lu pixels\n",local_name,local_batches,local_pixels);
    for(i=0;i<nworkers;i++)printf("Worker %s: %lu batches, %lu pixels, active=%d\n",workers[i].name,workers[i].batches,workers[i].pixels,workers[i].alive);
    free(expected);free(line);free(data);free(dx);free(dy);free(xp);free(yp);free_fractalc(fc);
    printf("%s distributed exact rendering and incremental row/column preservation\n",errors?"FAIL":"PASS");
    return errors!=0;
}
