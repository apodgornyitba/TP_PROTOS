/**
 * main.c - servidor proxy socks concurrente
 *
 * Interpreta los argumentos de línea de comandos, y monta un socket
 * pasivo.
 *
 * Todas las conexiones entrantes se manejarán en éste hilo.
 *
 * Se descargará en otro hilos las operaciones bloqueantes (resolución de
 * DNS utilizando getaddrinfo), pero toda esa complejidad está oculta en
 * el selector.
 */
/* Codigo provisto por la cátedra */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>   // socket
#include <sys/socket.h>  // socket
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "socks5.h"
#include "../inlcude/selector.h"
#include "socks5nio.h"

#define BUFF_SIZE 1025

static bool done = false;

static void
sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}


void receive(struct selector_key *key){
    printf("Received: %d\n", key->fd);
    
    char buffer[BUFF_SIZE]; 
    int valread;

    //closing socket
    if ((valread = read(key->fd , buffer, BUFF_SIZE-1)) == 0) {
        printf("Host disconnected: %d\n" , key->fd);
        selector_unregister_fd(key->s, key->fd);
    }
    //receiving message
    else {
        //set the string terminating NULL byte at the end of the data read
        buffer[valread] = '\0';
        printf("%s %d: %s", "Received from client", key->fd,  buffer);
    }
}

//based on old server's connection establishment
struct addrinfo *res;
void socksv5_passive_accept(struct selector_key *key){
    printf("Socks passive connection accepted: %d \n", key->fd);
    struct sockaddr_storage client_address;
    int addrlen = sizeof(client_address), new_socket;
    if((new_socket = accept(key->fd, (struct sockaddr *)&client_address, (socklen_t*)&addrlen)) < 0) {
        perror("ERROR: Unable to accept socks connection");
        exit(EXIT_FAILURE);
    }

    const struct fd_handler socksv5 = {
            .handle_read       = receive,
            .handle_write      = NULL,
            .handle_close      = NULL, 
    };

    selector_register(key->s, new_socket, &socksv5, OP_READ, NULL);

    int directed_new_socket = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    printf("Connecting...\n");
    if(connect(directed_new_socket,res->ai_addr,res->ai_addrlen) != 0){
        printf("ERROR: Unable to connect to fixed destination.");
        exit(EXIT_FAILURE);
    }
    const struct fd_handler directed_socksv5 = {
            .handle_read       = receive,
            .handle_write      = NULL,
            .handle_close      = NULL, 
    };
    printf("Connected to fixed destination!\n");
    selector_register(key->s, directed_new_socket, directed_socksv5, OP_WRITE, NULL);

}

int main(const int argc, const char **argv) {
    //create socket to destination
    struct addrinfo hints;

    //get host info, create socket and establish connection
    memset(&hints, 0,sizeof hints);
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if(getaddrinfo(IP_FOR_REQUESTS, PORT_FOR_REQUESTS, &hints, &res) != 0){
        printf("ERROR: getadrrinfo failed");
        exit(EXIT_FAILURE);
    }

    //new empty args struct
    socks5args args = malloc(sizeof(socks5args_struct));
    memset(args, 0, sizeof(*args));

    //get configurations and users
    parse_args(argc, argv, args);

    close(0);

    const char *err_msg     = NULL;
    selector_status ss      = SELECTOR_SUCCESS;
    fd_selector selector    = NULL;

    //get addr info
    memset(&(args->socks_addr_info), 0, sizeof((args->socks_addr_info)));
    (args->socks_addr_info).sin_family      = AF_INET;
    (args->socks_addr_info).sin_addr.s_addr = htonl(INADDR_ANY);
    (args->socks_addr_info).sin_port        = htons(args->socks_port);


    //a partir de aca - CODA
    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server < 0) {
        err_msg = "unable to create socket";
        goto finally;
    }

    fprintf(stdout, "Listening on TCP port %d\n", args->socks_port);

    /* No importa reportar nada si falla, man 7 ip. */
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    if(bind(server, (struct sockaddr*) &(args->socks_addr_info), sizeof(args->socks_addr_info)) < 0) {
        err_msg = "unable to bind socket";
        goto finally;
    }

    if (listen(server, MAX_CONNECTIONS_QUEUE) < 0) {
        err_msg = "unable to listen";
        goto finally;
    }

    /*  Registrar sigterm es Util para terminar el programa normalmente.
        Esto ayuda mucho en herramientas como valgrind.                         */
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    /* Check socket flags?          */
    if(selector_fd_set_nio(server) == -1) {
        err_msg = "getting server socket flags";
        goto finally;
    }

    const struct selector_init conf = {
            .signal = SIGALRM,
            .select_timeout = {
                    .tv_sec  = 10,
                    .tv_nsec = 0,
            },
    };

    if(0 != selector_init(&conf)) {
        err_msg = "initializing selector";
        goto finally;
    }

    selector = selector_new(1024);
    if(selector == NULL) {
        err_msg = "unable to create selector";
        goto finally;
    }

    const struct fd_handler socksv5 = {
            .handle_read       = socksv5_passive_accept,
            .handle_write      = NULL,
            .handle_close      = NULL, // nada que liberar
    };

    ss = selector_register(selector, server, &socksv5,
                           OP_READ, NULL);
    if(ss != SELECTOR_SUCCESS) {
        err_msg = "registering fd";
        goto finally;
    }

    for(;!done;) {
        err_msg = NULL;
        ss = selector_select(selector);
        if(ss != SELECTOR_SUCCESS) {
            err_msg = "serving";
            goto finally;
        }
    }

    if(err_msg == NULL) {
        err_msg = "closing";
    }

    int ret = 0;
    finally:
    if(ss != SELECTOR_SUCCESS) {
        fprintf(stderr, "%s: %s\n", (err_msg == NULL) ? "": err_msg,
                ss == SELECTOR_IO
                ? strerror(errno)
                : selector_error(ss));
        ret = 2;
    } else if(err_msg) {
        perror(err_msg);
        ret = 1;
    }
    if(selector != NULL) {
        selector_destroy(selector);
    }
    selector_close();

//    socksv5_pool_destroy();

    if(server >= 0) {
        close(server);
    }

    return ret;
}
