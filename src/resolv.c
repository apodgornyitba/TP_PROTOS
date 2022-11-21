#include "../include/resolv.h"
#include <string.h>
#include <stdio.h>

void *request_resolv_blocking(void *ptr) {
    struct selector_key *key = (struct selector_key *) ptr;
    struct socks5 *data = ATTACHMENT(key);
    struct request *request_data = data->client.request.parser->request;
    struct sockaddr_fqdn fqdn = data->client.request.parser->request->fqdn;

    /**
     * The pthread_detach() function is used to indicate to the implementation
     * that storage for the thread thread can be reclaimed when the thread terminates.
     */
    pthread_detach(pthread_self());

    data->origin_resolution = 0;

    //// Hints for getaddrinfo

    struct addrinfo hints = {
            .ai_family=AF_UNSPEC,
            .ai_socktype=SOCK_STREAM,
            .ai_flags= AI_PASSIVE,
            .ai_protocol=0,
            .ai_canonname=NULL,
            .ai_addr=NULL,
            .ai_next=NULL,
    };

    //// Set port
    char buff[7];
    uint16_t port = ntohs(request_data->dest_port);
    snprintf(buff, sizeof(buff), "%d", port);

    //// Just debugging
    char *buffer = malloc((fqdn.size + 1));
    for (int i = 0; i < fqdn.size; ++i) {
        buffer[i] = fqdn.host[i];
    }
    buffer[fqdn.size] = 0;

    getaddrinfo(buffer, buff, &hints, &data->origin_resolution);
    free(buffer);
    buffer = NULL;

    selector_notify_block(key->s, key->fd);

    free(key);
    key = NULL;
    return 0;
}

unsigned request_resolv_done(struct selector_key *key) {
    struct socks5 *data = ATTACHMENT(key);

    if (data->origin_resolution == NULL) {
        data->orig.conn.status = status_general_socks_server_failure;
        data->client.request.status=status_general_socks_server_failure;
        return error_handler(data->client.request.status, key);
    }

    //// Seteo current
    struct addrinfo *current = data->origin_resolution_current = data->origin_resolution;

    //// IPv4
    if (current->ai_family == AF_INET) {
        data->origin_domain = AF_INET;
        data->origin_addr_len = sizeof(struct sockaddr);
        memcpy((struct sockaddr *) &(data->origin_addr), current->ai_addr, sizeof(struct sockaddr));
    }

    //// IPv6
    if (current->ai_family == AF_INET6) {
        data->origin_domain = AF_INET6;
        data->origin_addr_len = sizeof(struct sockaddr_in6);
        memcpy((struct sockaddr_in6 *) &(data->origin_addr), current->ai_addr, sizeof(struct sockaddr_in6));
    }
    return REQUEST_CONNECTING;
}