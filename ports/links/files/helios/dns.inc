/* An independent Helios resolver task writes a bounded HUR1 frame. The UI
 * only reads available bytes: a slow DNS lookup must never stop keyboard I/O.
 * Links' existing SIGCHLD handler reaps these children. */
static void end_real_lookup(void *data)
{
    struct dnsquery *q=data;
    int n, i, error=1;
    unsigned char *w=q->helios_reply;
    n=read(q->h,(char *)w+q->helios_received,392-q->helios_received);
    if(n<0 && (errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK)) return;
    if(n>0) {
        q->helios_received+=n;
        if(q->helios_received<392) return;
        if(q->addr && !memcmp(w,"HUR1",4) && !w[4] && w[5]>0 &&
           w[5]<=32 && !w[6] && !w[7] && !w[391]) {
            memset(q->addr,0,sizeof(*q->addr));
            for(i=0;i<w[5];i++)
                add_address(q->addr,AF_INET,w+8+4*i,0,q->addr_preference);
            error=!q->addr->n;
        }
    }
    set_handlers(q->h,NULL,NULL,NULL);
    close(q->h);
    end_dns_lookup(q,error,-1);
}

static void do_lookup(struct dnsquery *q,int force_async)
{
    int fds[2], error;
    pid_t pid;
    unsigned char numeric[4];
    if(!q->addr) { end_dns_lookup(q,1,-1);return; }
    if(!numeric_ip_address(q->name,numeric)) {
        memset(q->addr,0,sizeof(*q->addr));
        add_address(q->addr,AF_INET,numeric,0,q->addr_preference);
        end_dns_lookup(q,0,-1);
        return;
    }
    if(hu_channel(fds)<0) { end_dns_lookup(q,1,-1);return; }
    error=hu_resolve_launch(&pid,fds[1],(char *)q->name,0,NULL);
    close(fds[1]);
    if(error || hu_set_nonblock(fds[0],1)<0) {
        close(fds[0]);end_dns_lookup(q,1,-1);return;
    }
    q->h=fds[0];q->helios_received=0;
    set_handlers(q->h,end_real_lookup,NULL,q);
}
