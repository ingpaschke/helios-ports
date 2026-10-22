/* Resolver runs as an independent Helios task. Only a bounded HUR1 value
 * message crosses the /internet-backed channel; GLib stays in this task. */
int net_gethostbyname_nonblock(const char *addr,GIOChannel *channel,int reverse_lookup)
{
    pid_t pid;int fd=g_io_channel_unix_get_fd(channel);
    int error=hu_resolve_launch(&pid,fd,addr,reverse_lookup,NULL);
    g_io_channel_close(channel); /* child now owns the only writer */
    if(error) {
        /* Closing the write direction wakes the reader, which reports failure.
         * Do not leave a lookup hanging because a child could not start. */
        errno=error;return -1;
    }
    pidwait_add(pid);return pid;
}
int net_gethostbyname_return(GIOChannel *channel,RESOLVED_IP_REC *rec)
{
    struct hu_resolve_result result;
    memset(rec,0,sizeof(*rec));
    if(hu_resolve_receive(g_io_channel_unix_get_fd(channel),&result)<0) {
        rec->error=-1;rec->errorstr=g_strdup_printf("Host name lookup: %s",g_strerror(errno));return -1;
    }
    if(result.error) {
        rec->error=-1;rec->errorstr=g_strdup(gai_strerror(result.error));return 0;
    }
    rec->ip4.family=AF_INET;memcpy(&rec->ip4.ip,result.addresses[0],4);
    if(result.canonical[0])rec->host4=g_strdup(result.canonical);
    return 0;
}
