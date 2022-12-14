#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "../include/connecting.h"
#include "../include/request.h"
#include "netutils.h"
#include <time.h>

#define IPV4_LEN 4
#define IPV6_LEN 16
#define MAX_IP_LENGTH 45
extern const struct fd_handler socks5_handler;

enum socks_reply_status connection(struct selector_key *key);
enum socks_reply_status errno_to_socks(int e);

void connecting_init(const unsigned state, struct selector_key *key){
    struct socks5 * data = ATTACHMENT(key);
    data->orig.conn.wb = &data->write_buffer;
    data->orig.conn.client_fd = data->client_fd;
    data->orig.conn.origin_fd = -1;
    data->orig.conn.status=status_succeeded;

    int *fd= &data->origin_fd;

    *fd= socket(ATTACHMENT(key)->origin_domain, SOCK_STREAM, 0);

    if(*fd < 0){
        data->orig.conn.status=status_general_socks_server_failure;
        error_handler_to_client(data->orig.conn.status, key);
        return;
    }

    // Socket no bloqueante
    int flag_setting = selector_fd_set_nio(*fd);
    if(flag_setting == -1) {
        data->orig.conn.status=status_general_socks_server_failure;
        error_handler_to_client(data->orig.conn.status, key);
        return;
    }

    data->orig.conn.status=connection(key);
}

extern struct users users[MAX_USERS];
extern size_t metrics_historic_connections;
extern size_t metrics_concurrent_connections;
extern size_t metrics_max_concurrent_connections;

unsigned connecting_write(struct selector_key *key){
    struct socks5 * data = ATTACHMENT(key);
    if(data->orig.conn.status != status_succeeded){
        if(data->origin_resolution_current != NULL && data->origin_resolution_current->ai_next != NULL){

            struct addrinfo * current = data->origin_resolution_current = data->origin_resolution_current->ai_next;

            //// IPv4
            if(current->ai_family == AF_INET)
                memcpy((struct sockaddr_in *) &(data->origin_addr), current->ai_addr, sizeof(struct sockaddr_in));

            //// IPv6
            if(current->ai_family == AF_INET6)
                memcpy((struct sockaddr_in6 *) &(data->origin_addr), current->ai_addr, sizeof(struct sockaddr_in6));

            connection(key);

            return REQUEST_CONNECTING;//vuelve a probar la siguiente ip

        }
        selector_unregister_fd(key->s, data->origin_fd);
        close(data->origin_fd);
        data->origin_fd=-1;
        return error_handler_to_client(data->orig.conn.status, key);
    }

    struct sockaddr * origAddr = (struct sockaddr *) &ATTACHMENT(key)->origin_addr;
    struct sockaddr * clientAddr = (struct sockaddr *) &ATTACHMENT(key)->client_addr;
    char * orig = malloc(MAX_IP_LENGTH);
    char * client = malloc(MAX_IP_LENGTH);


    int error;
    socklen_t len= sizeof(error);

    if(getsockopt(key->fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0){
        data->orig.conn.status=status_general_socks_server_failure;
        time_t now;
        time(&now);
        char buf[sizeof "2011-10-08T07:07:09Z"];
        strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
        printf("%s\t%s\tRegister A\t to: %s \t from: %s\t status: %d\n", buf, users[ATTACHMENT(key)->userIndex].name, sockaddr_to_human(orig, MAX_IP_LENGTH, origAddr), sockaddr_to_human(client, MAX_IP_LENGTH, clientAddr), data->orig.conn.status);
        return error_handler_to_client(data->orig.conn.status, key);
    }

    if(error== 0){  
        metrics_historic_connections += 1;
        metrics_concurrent_connections += 1;
        if(metrics_concurrent_connections > metrics_max_concurrent_connections)
            metrics_max_concurrent_connections = metrics_concurrent_connections;
        time_t now;
        time(&now);
        char buf[sizeof "2011-10-08T07:07:09Z"];
        strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
        printf("%s\t%s\tRegister A\t to: %s \t from: %s\t status: %d\n", buf, users[ATTACHMENT(key)->userIndex].name, sockaddr_to_human(orig, MAX_IP_LENGTH, origAddr), sockaddr_to_human(client, MAX_IP_LENGTH, clientAddr), data->orig.conn.status);
        if(data->client.request.addr_family == socks_req_addrtype_domain)
            freeaddrinfo(data->origin_resolution);
        data->orig.conn.status=status_succeeded;
        data->orig.conn.origin_fd = key->fd;
    }
    else{
        if(data->origin_resolution_current==NULL){
            data->orig.conn.status= errno_to_socks(error);
            time_t now;
            time(&now);
            char buf[sizeof "2011-10-08T07:07:09Z"];
            strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
            printf("%s\t%s\tRegister A\t to: %s \t from: %s \t status: %d\n", buf, users[ATTACHMENT(key)->userIndex].name, sockaddr_to_human(orig, MAX_IP_LENGTH, origAddr), sockaddr_to_human(client, MAX_IP_LENGTH, clientAddr), data->orig.conn.status);
            return error_handler_to_client(data->orig.conn.status, key);
        }

        data->orig.conn.status = errno_to_socks(error);

        if(data->origin_resolution_current != NULL && data->origin_resolution_current->ai_next != NULL){

            struct addrinfo * current = data->origin_resolution_current = data->origin_resolution_current->ai_next;

            //// IPv4
            if(current->ai_family == AF_INET)
                memcpy((struct sockaddr_in *) &(data->origin_addr), current->ai_addr, sizeof(struct sockaddr_in));

            //// IPv6
            if(current->ai_family == AF_INET6)
                memcpy((struct sockaddr_in6 *) &(data->origin_addr), current->ai_addr, sizeof(struct sockaddr_in6));

            connection(key);
            return REQUEST_CONNECTING;

        } else{
            data->orig.conn.status=errno_to_socks(error);
            time_t now;
            time(&now);
            char buf[sizeof "2011-10-08T07:07:09Z"];
            strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
            printf("%s\t%s\tRegister A\t to: %s \t from: %s \t status: %d\n",buf, users[ATTACHMENT(key)->userIndex].name, sockaddr_to_human(orig, MAX_IP_LENGTH, origAddr), sockaddr_to_human(client, MAX_IP_LENGTH, clientAddr), data->orig.conn.status);
            if(data->client.request.addr_family == socks_req_addrtype_domain)
                freeaddrinfo(data->origin_resolution);
            return error_handler_to_client(data->orig.conn.status, key);
        }
    }

    int request_marshall_result = request_marshall(data->orig.conn.status, &data->write_buffer);
    if(request_marshall_result == -1){
        return ERROR;
    }

    selector_status s=0;
    s|= selector_set_interest(key->s, data->orig.conn.client_fd, OP_WRITE);
    s|= selector_set_interest_key(key, OP_NOOP);
    return SELECTOR_SUCCESS == s ? REQUEST_WRITE:ERROR;
}

//// CLOSE
void connecting_close(const unsigned state, struct selector_key *key){
    struct request_st *d = &ATTACHMENT(key)->client.request;
    if(d->parser != NULL) {
        request_parser_close(d->parser);
        free(d->parser);
    }
}


enum socks_reply_status errno_to_socks(int e){
    enum socks_reply_status ret;

    switch (e)
    {
        case 0:
            ret = status_succeeded;
            break;
        case ECONNREFUSED:
            ret = status_connection_refused;
            break;
        case EHOSTUNREACH:
            ret = status_host_unreachable;
            break;
        case ENETUNREACH:
            ret = status_network_unreachable;
            break;
        case ETIMEDOUT:
            ret = status_ttl_expired;
            break;
        default:
            ret = status_general_socks_server_failure;
            break;
    }

    return ret;
}

extern size_t metrics_historic_connections_attempts;
enum socks_reply_status connection(struct selector_key *key){
    struct socks5 * data = ATTACHMENT(key);

    int *fd= &data->origin_fd;

    metrics_historic_connections_attempts += 1;

    int connectResult = connect(*fd, (const struct sockaddr*)&ATTACHMENT(key)->origin_addr, ATTACHMENT(key)->origin_addr_len);

    if(connectResult != 0 && errno != EINPROGRESS){
        data->client.request.status = errno_to_socks(errno);
        data->orig.conn.status=errno_to_socks(errno);
        error_handler_to_client(data->orig.conn.status, key);
        return data->orig.conn.status;
    }

    if(connectResult != 0){
        selector_status st= selector_set_interest_key(key, OP_NOOP);
        if(SELECTOR_SUCCESS != st){
            data->orig.conn.status=status_general_socks_server_failure;
            error_handler_to_client(data->orig.conn.status, key);
            return data->orig.conn.status;
        }

        st= selector_register(key->s, *fd, &socks5_handler,OP_WRITE, key->data);
        if(SELECTOR_SUCCESS != st){
            data->orig.conn.status=status_general_socks_server_failure;
            error_handler_to_client(data->orig.conn.status, key);
            close((*fd));
            *fd=-1;
            return data->orig.conn.status;
        }
        ATTACHMENT(key)->references += 1;           
    }
    else{
        ATTACHMENT(key)->references += 1;           
        selector_status st= selector_set_interest_key(key, OP_READ);
        if(SELECTOR_SUCCESS != st){
            data->orig.conn.status=status_general_socks_server_failure;
            error_handler_to_client(data->orig.conn.status, key);
            return data->orig.conn.status;
        }

        st= selector_register(key->s, *fd, &socks5_handler,OP_READ, key->data);
        if(SELECTOR_SUCCESS != st){
            data->orig.conn.status=status_general_socks_server_failure;
            error_handler_to_client(data->orig.conn.status, key);
            return data->orig.conn.status;
        }
    }


    return status_succeeded;
   
}