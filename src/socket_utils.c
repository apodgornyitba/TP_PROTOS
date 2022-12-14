#include "../include/socket_utils.h"

#define ERROR -1
#define OK 0
#define BACKLOG 20

char * err_msg;
int addr_process(int server, struct sockaddr_in * addr){
    if(bind(server, (struct sockaddr*) addr, sizeof(*addr)) < 0) {
        err_msg = "unable to bind socket";
        printf("%s\n", err_msg);
        return ERROR;
    }
    return OK;
}

int opt = 1;
int addr6_process(int server, struct sockaddr_in6 * addr){
    if (setsockopt(server, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&opt, sizeof(int)) < 0) {
        printf("Failed to set IPV6_V6ONLY\n");
        return ERROR;
    }
    if(bind(server, (struct sockaddr*) addr, sizeof(*addr)) < 0) {
        err_msg = "unable to bind socket";
        printf("%s\n", err_msg);
        return ERROR;
    }
    return OK;
}

int create_socket(int type, struct sockaddr_in * addr, struct sockaddr_in6 * addr6){
    if(type != AF_INET && type != AF_INET6){
        err_msg = "Wrong socket type";
        printf("%s\n", err_msg);
        return ERROR;
    }
    int server;
    if(type == AF_INET) {
       /* Creando socket de IPv4 */
        server = socket(type, SOCK_STREAM, IPPROTO_TCP);
    }
    else {
        /* Creando socket de IPv6 */
        server = socket(type, SOCK_STREAM, IPPROTO_TCP);
    }

    if(server < 0) {
        err_msg = "unable to create socket";
        printf("%s\n", err_msg);
        return ERROR;
    }

    // man 7 ip
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    if(type == AF_INET) {

        if (addr_process(server, addr) == ERROR) {
            return ERROR;
        }
    }
    if(type == AF_INET6) {
        if(addr6_process(server, addr6) == ERROR)
            return ERROR;
    }

    if (listen(server, BACKLOG) < 0) {
        err_msg = "unable to listen";
        printf("%s\n", err_msg);
        return ERROR;
    }
    if(selector_fd_set_nio(server) == -1) {
        err_msg = "getting server socket flags";
        printf("%s\n", err_msg);
        return ERROR;
    }
    return server;
}