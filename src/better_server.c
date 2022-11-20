#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#include "../include/buffer.h"
#include "../include/selector.h"
#include "../include/args.h"
#include <sys/types.h>
// #include "socks5.h"
#include <signal.h>

#define BUFFER_SIZE 10
#define MAX_CONNECTIONS_QUEUE 20
#define IP_FOR_REQUESTS "127.0.0.1"
#define PORT_FOR_REQUESTS "3000"

static bool done = false;
static void sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

struct information{
    int fdClient;   // Uses normal buffers
    int fdOrigin;   // Uses inverse buffers

    // Buffers where data is stored
    buffer * read_buffer;
    buffer * write_buffer;

    // Current state
    unsigned state;
}information;

struct addrinfo *res;
void socksv5_read(struct selector_key *key);
void socksv5_write(struct selector_key *key);

void debug(char * label, char * message, int num){
    printf("Debug, %s: %s %d\n", label, message, num);
}

const struct fd_handler socksv5 = {
        .handle_read       = socksv5_read,
        .handle_write      = socksv5_write,
//        .handle_close      = socksv5_close,
//        .handle_block      = socksv5_block,
};

void setNewInterests( fd_selector selector, int fd, buffer * read_buffer, buffer * write_buffer){
    fd_interest i = OP_NOOP;
    if(buffer_can_write(read_buffer))
        i = i | OP_READ;
    if(buffer_can_read(write_buffer))
        i = i | OP_WRITE;
    selector_set_interest(selector, fd, i);
}

void socksv5_read(struct selector_key *key){
    char * label = "socksv5_read";
    struct information * myInfo = key->data;
    int fdRead, fdWrite;
    buffer * buffer_r, * buffer_w;
    if(myInfo->fdClient == key->fd){
        fdRead = myInfo->fdClient;
        fdWrite = myInfo->fdOrigin;
        buffer_r = myInfo->read_buffer;
        buffer_w = myInfo->write_buffer;
    } else{
        fdWrite = myInfo->fdClient;
        fdRead = myInfo->fdOrigin;
        buffer_r = myInfo->write_buffer;
        buffer_w = myInfo->read_buffer;
    }
    // Check whether buffer has capacity
    if(!buffer_can_write(buffer_r)){
        debug(label, "Buffer de lectura lleno y estoy suscripto para leer!", 0);
        return;
    }

    // Read
    debug(label, "Comienzo lectura", 0);
    size_t nbytes;
    uint8_t * ptr = buffer_write_ptr(buffer_r, &nbytes);
    ssize_t received = recv(key->fd, ptr, nbytes, 0);
    if(received > 0 ) { // Llego algo
        debug(label, "Leí normal", received);
        buffer_write_adv(buffer_r, received);
    }
    else{              
        debug(label, "Leí un EOF. Termino mi suscripción del el fd.", 0);
        printf("Unregister fd %d", key->fd);
        selector_unregister_fd(key->s, key->fd);
        return;
    }
    setNewInterests(key->s, fdRead, buffer_r, buffer_w);
    setNewInterests(key->s, fdWrite, buffer_w, buffer_r);
}

void socksv5_write(struct selector_key *key){
    char * label = "Escritura";
    struct information * myInfo = key->data;
    int fdRead, fdWrite;
    buffer * buffer_r, * buffer_w;
    int i;
    if(myInfo->fdClient == key->fd){
        fdRead = myInfo->fdClient;
        fdWrite = myInfo->fdOrigin;
        buffer_r = myInfo->read_buffer;
        buffer_w = myInfo->write_buffer;
    } else{
        fdWrite = myInfo->fdClient;
        fdRead = myInfo->fdOrigin;
        buffer_r = myInfo->write_buffer;
        buffer_w = myInfo->read_buffer;
    }
    // Check whether there is something to write
    if(!buffer_can_read(buffer_w)) {
        debug(label, "No tengo nada para leer y estoy suscrito a escritura!", 0);
        return;
    }

    // Write
    debug(label, "Comienzo escritura", 0);
    size_t nbytes;
    uint8_t * ptr = buffer_read_ptr(buffer_w, &nbytes);
    ssize_t received = send(key->fd, ptr, nbytes, 0);
    if(received > 0 ) { 
        debug(label, "Escritura completada normalmente",received);
        buffer_read_adv(buffer_w, received);
    }
    // Close connection
    else{              
        debug(label, "No escribí nada?",0);
        return;
    }

    setNewInterests(key->s, fdRead, buffer_r, buffer_w);
    setNewInterests(key->s, fdWrite, buffer_w, buffer_r);
}

void socksv5_passive_accept(struct selector_key *key){

    struct information * new_information = malloc(sizeof(information));

    // Accept connection
    printf("Socks passive accept %d \n", key->fd);
    struct sockaddr_storage client_address;
    int addrlen = sizeof(client_address), new_socket;
    if ((new_socket = accept(key->fd, (struct sockaddr *)&client_address, (socklen_t*)&addrlen))<0)
    {
        perror("accept error");
        exit(EXIT_FAILURE);
    }
    // Check socket flags
    if(selector_fd_set_nio(new_socket) == -1) {
        printf("error getting server socket flags");
        return;
    }
    // Non blocking socket
    new_information->fdClient = new_socket;

    // Connect to fixed destination
    printf("Connecting to fixed destination...\n");
    int D_new_socket = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
    // Non blocking socket
    if(selector_fd_set_nio(D_new_socket) == -1) {
        printf("getting server socket flags");
        return;
    }
    int connectResult = connect(D_new_socket,res->ai_addr,res->ai_addrlen);
    if(connectResult != 0 && errno != 36){
        printf( "Connect to fixed destination failed. %d\n", errno);
        exit(EXIT_FAILURE);
    }
    new_information->fdOrigin = D_new_socket;
    printf("Connected! :) \n");

    // Initialize buffer
    printf("Inicializo buffers\n");
    // Client buffer
    new_information->read_buffer = malloc(sizeof(buffer));
    buffer_init( (new_information->read_buffer), BUFFER_SIZE + 1, malloc(BUFFER_SIZE + 1));
    // Origin buffer
    new_information->write_buffer = malloc(sizeof(buffer));
    buffer_init( (new_information->write_buffer), BUFFER_SIZE + 1, malloc(BUFFER_SIZE + 1));

    // Suscriptions
    selector_register(key->s, new_socket, &socksv5, OP_READ, new_information); 
    selector_register(key->s, D_new_socket, &socksv5, OP_READ, new_information); 
}

int main(const int argc, const char **argv) {

    //// Fixed destination
    struct addrinfo hints;
    //get host info, make socket and connect it
    memset(&hints, 0,sizeof hints);
    hints.ai_family=AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if(getaddrinfo(IP_FOR_REQUESTS, PORT_FOR_REQUESTS, &hints, &res) != 0){
        printf( "getadrrinfo failed %d", errno);
        exit(EXIT_FAILURE);
    }
    printf("Fixed address obtained\n");

    /*  New empty args struct           */
    socks5args args = malloc(sizeof(socks5args_struct));
    memset(args, 0, sizeof(*args));

    /*  Get configurations and users    */
    parse_args(argc, argv, args);

    close(0);

    const char       *err_msg = NULL;
    selector_status   ss      = SELECTOR_SUCCESS;
    fd_selector selector      = NULL;

    /* Get addr info                    */
    memset(&(args->socks_addr_info), 0, sizeof((args->socks_addr_info)));
    (args->socks_addr_info).sin_family      = AF_INET;
    (args->socks_addr_info).sin_addr.s_addr = htonl(INADDR_ANY);
    (args->socks_addr_info).sin_port        = htons(args->socks_port);

    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server < 0) {
        err_msg = "unable to create socket";
        goto finally;
    }

    fprintf(stdout, "Listening on TCP port %d\n", args->socks_port);

    /* No importa reportar nada si falla, man 7 ip. */
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));
//    fcntl(server, F_SETFL, O_NONBLOCK);  // set to non-blocking

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

    //// Acá me estoy suscribiendo directo
    ss = selector_register(selector, server, &socksv5, OP_READ, NULL);
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
