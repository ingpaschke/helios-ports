/* The selected TLS backend performs the handshake. Poll status using a
 * Links timer; never block the terminal's event loop or retry in plaintext.
 * free_connection_data cancels this timer before freeing the connection. */
static void helios_tls_poll(void *arg)
{
    struct connection *c=arg;
    struct conn_info *b=c->newconn;
    struct hu_tls_info info;
    int result;
    c->helios_tls_timer=NULL;
    result=hu_tls_fd_handshake(*b->sock, &info);
    if (result==0) {
        c->helios_tls_timer=install_timer(25, helios_tls_poll, c);
        return;
    }
    if (result<0) {
        setcstate(c, S_SSL_ERROR);
        abort_connection(c);
        return;
    }
    connected_callback(c);
}
