/* TLS owns encryption and readiness. The GLib Unix-channel adapter routes
 * read/write/select/close through hu_tls_fd, including buffered plaintext and
 * opposite-direction waits. connect_tag still owns cancellation of this timer. */
#include <helios/unix/tls_fd.h>

static int helios_tls_poll(void *data)
{
    SERVER_REC *server = data;
    struct hu_tls_info info;
    int fd = g_io_channel_unix_get_fd(server->handle->handle);
    int result = hu_tls_fd_handshake(fd, &info);
    char *message;

    if (result == 0) return TRUE;
    /* GLib removes the current timer when we return FALSE. Clear the tag
     * before callbacks can destroy the server or try removing it again. */
    server->connect_tag = -1;
    if (result < 0 || info.state != HU_TLS_READY) {
        message = g_strdup_printf("TLS: %s", info.message[0] ? info.message :
            (result < 0 ? g_strerror(errno) : "peer closed during handshake"));
        server->connection_lost = TRUE;
        server_connect_failed(server, message);
        g_free(message);
    } else {
        lookup_servers = g_slist_remove(lookup_servers, server);
        server_connect_finished(server);
    }
    return FALSE;
}

static void helios_tls_begin(SERVER_REC *server, GIOChannel *handle)
{
    int fd = g_io_channel_unix_get_fd(handle);
    char *message;
    if (hu_tls_fd_start(fd, server->connrec->address, 30000) == 0) {
        server->connect_tag = g_timeout_add(50, helios_tls_poll, server);
        return;
    }
    message = g_strdup_printf("TLS: %s", g_strerror(errno));
    server->connection_lost = TRUE;
    server->no_reconnect = TRUE;
    server_connect_failed(server, message);
    g_free(message);
}

static const char *helios_tls_options(SERVER_CONNECT_REC *conn)
{
    if (!conn->use_ssl) return NULL;
    if (conn->proxy != NULL || conn->unix_socket)
        return "TLS: use a direct TCP connection (no IRC proxy or Unix socket)";
    if ((conn->ssl_cert && *conn->ssl_cert) || (conn->ssl_pkey && *conn->ssl_pkey) ||
        (conn->ssl_cafile && *conn->ssl_cafile) || (conn->ssl_capath && *conn->ssl_capath))
        return "TLS: certificates use the TLS backend trust store; client certificates and per-server CA files are unsupported";
    return NULL;
}
