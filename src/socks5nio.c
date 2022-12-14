/* socks5nio.c  - controla el flujo de un proxy SOCKSv5 (sockets no bloqueantes) */
/* Codigo provisto por la cátedra (Juan Codagnone) */
/* MODIFICADO */

#include <netdb.h>
#include <stdlib.h>  // malloc
#include <string.h>  // memset
#include <unistd.h>  // close
#include <arpa/inet.h>
#include "../include/hello.h"
#include "../include/authentication.h"
#include "../include/buffer.h"
#include "../include/connecting.h"
#include "../include/copy.h"
#include "../include/resolv.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

const struct fd_handler socks5_handler = {
        .handle_read   = socksv5_read,
        .handle_write  = socksv5_write,
        .handle_close  = socksv5_close,
        .handle_block  = socksv5_block,
};

/* definición de handlers para cada estado */
static const struct state_definition client_statbl[] = {
        {
                .state = HELLO_READ,
                .on_arrival = hello_read_init,
                .on_departure = hello_read_close,
                .on_read_ready = hello_read,
        },
        {       .state = HELLO_WRITE,
                .on_arrival = hello_write_init,
                .on_departure = hello_write_close,
                .on_write_ready = hello_write

        },
        {
                .state = USERPASS_READ,
                .on_arrival = auth_read_init,
                .on_departure = auth_read_close,
                .on_read_ready = auth_read,
        },
        {
                .state = USERPASS_WRITE,
                .on_arrival = auth_write_init,
                .on_write_ready = auth_write,
                .on_departure = auth_write_close,
        },
        {
                .state = REQUEST_READ,
                .on_arrival = request_init,
                .on_departure = request_close,
                .on_read_ready = request_read,
        },
        {
                .state = REQUEST_RESOLV,
                .on_block_ready= request_resolv_done,
        },
        {
                .state = REQUEST_CONNECTING,
                .on_arrival = connecting_init,
                // .on_read_ready = connecting_read,
                .on_write_ready = connecting_write,
                .on_departure = connecting_close,
        },
        {
                .state = REQUEST_WRITE,
                // .on_arrival = request_init,
                // .on_departure = request_close,
                .on_write_ready = request_write,
        },
        {       .state = COPY,
                .on_arrival = copy_init,
                .on_read_ready = copy_read,
                .on_write_ready = copy_write,
//                .on_departure = copy_close
        },
        {
                .state = DONE,
                // For now, no need to define any handlers, all in sockv5_done
        },
        {
                .state = ERROR,
                // No now, no need to define any handlers, all in sockv5_done
        }
};


/* Pool de struct socks5, para ser reusados.
 * Como tenemos un unico hilo que emite eventos no necesitamos barreras de
 * contención. */

static const unsigned max_pool=50;
static unsigned  pool_size=0;
static struct socks5 *pool=0;

// static const struct  state_definition* socks5_describe_states();

/** crea un nuevo struct socks */
static struct socks5* socks5_new(int client_fd){
    struct socks5 *ret;

    if(pool == NULL){
        ret = malloc(sizeof(*ret));
    }else{
        ret=pool;
        pool=pool->next;
        ret->next=0;
    }

    if(ret == NULL){
        goto finally;
    }

    memset(ret, 0x00, sizeof(*ret));

    ret->isSocks = true;

    ret->origin_fd =-1;
    ret->client_fd= client_fd;
    ret->client_addr_len= sizeof(ret->client_addr);

    // INITIAL STATE
    ret->stm.initial =HELLO_READ;
    ret->stm.max_state= ERROR;
    ret->stm.current= &client_statbl[0];
    ret->stm.states= client_statbl;
    stm_init(&ret->stm);

    ret->done_state = DONE;
    ret->error_state = ERROR;

    password_parser_init(&ret->password_parser);
    ret->password_parser.userIndex = &ret->userIndex;
    ret->password_parser.client = &ret->client_addr;
    ret->password_parser.origin = &ret->origin_addr;

    buffer_init(&ret->read_buffer, N(ret->raw_buff_a), ret->raw_buff_a);
    buffer_init(&ret->write_buffer, N(ret->raw_buff_b), ret->raw_buff_b);

    ret->references= 1;
    return ret;
    finally:
        return ret;
}

/** realmente destruye */
static void socks5_destroy_(struct socks5* s) {
    free(s);
}

/* destruye un  `struct socks5', tiene en cuenta las referencias
 * y el pool de objetos. */
static void socks5_destroy(struct socks5 *s) {
    if(s == NULL) {
        // nada para hacer
    } else if(s->references == 1) {
        if(s != NULL) {
            if(pool_size < max_pool) {
                s->next = pool;
                pool    = s;
                pool_size++;
            } else {
                socks5_destroy_(s);
            }
        }
    } else {
        s->references -= 1;
    }
}

void socksv5_pool_destroy(void) {
    struct socks5 *next, *s;
    for(s = pool; s != NULL ; s = next) {
        next = s->next;
        free(s);
    }
}

/** Intenta aceptar la nueva conexión entrante*/
void socksv5_passive_accept(struct selector_key *key) {
    struct sockaddr_storage       client_addr;
    socklen_t                     client_addr_len = sizeof(client_addr);
    struct socks5                *state           = NULL;

    const int client = accept(key->fd, (struct sockaddr*) &client_addr, &client_addr_len);
    /* Accept connection */
    if(client == -1) {
        goto fail;
    }
    if(selector_fd_set_nio(client) == -1) {
        goto fail;
    }
    state = socks5_new(client);
    if(state == NULL) {
        // sin un estado, nos es imposible manejaro.
        // tal vez deberiamos apagar accept() hasta que detectemos
        // que se liberÃ³ alguna conexiÃ³n.
        goto fail;
    }
    memcpy(&state->client_addr, &client_addr, client_addr_len);
    state->client_addr_len = client_addr_len;
    selector_status ss = selector_register(key->s, client, &socks5_handler, OP_READ, state);
    if(SELECTOR_SUCCESS != ss) {
        goto fail;
    }
    return;
    fail:
    if(client != -1) {
        close(client);
    }
    socks5_destroy(state);
}

///////////////////////////////////////////////////////////////////////////////
// Handlers top level de la conexiÃ³n pasiva.
// son los que emiten los eventos a la maquina de estados.
        void socksv5_done(struct selector_key* key);

        void socksv5_read(struct selector_key *key) {
            struct state_machine *stm   = &ATTACHMENT(key)->stm;
            const enum socks_v5state st = stm_handler_read(stm, key);

            if(ERROR == st || DONE == st) {
                socksv5_done(key);
            }
        }

        void socksv5_write(struct selector_key *key) {
            struct state_machine *stm   = &ATTACHMENT(key)->stm;
            const enum socks_v5state st = stm_handler_write(stm, key);

            if(ERROR == st || DONE == st) {
                socksv5_done(key);
            }
        }

        void socksv5_block(struct selector_key *key) {
            struct state_machine *stm   = &ATTACHMENT(key)->stm;
            const enum socks_v5state st = stm_handler_block(stm, key);

            if(ERROR == st || DONE == st) {
                socksv5_done(key);
            }
        }

        void socksv5_close(struct selector_key *key) {
            socks5_destroy(ATTACHMENT(key));
        }

        void socksv5_done(struct selector_key* key) {
            const int fds[] = {
                    ATTACHMENT(key)->client_fd,
                    ATTACHMENT(key)->origin_fd,
            };
            for(unsigned i = 0; i < N(fds); i++) {
                if(fds[i] != -1) {
                    if(SELECTOR_SUCCESS != selector_unregister_fd(key->s, fds[i])) {
                        abort();
                    }
                    close(fds[i]);
                }
            }
        }