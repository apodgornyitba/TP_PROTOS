#include "../include/authentication.h"
#include "../include/selector.h"
#include "../include/new_parser.h"
#include "../include/states.h"
#include "../include/socks5nio.h"
#include "../include/parser.h"
#include <string.h>
#include <stdlib.h>

/*https://www.rfc-editor.org/rfc/rfc1929*/
/*https://stackoverflow.com/questions/6054092/how-to-add-authentication-to-a-socks5-proxy-server*/
/*https://stackoverflow.com/questions/49564243/socks5-connection-authentication*/
extern struct users users[MAX_USERS];
extern int nusers;
extern struct users admins[MAX_USERS];
extern int nadmins;

#define VERSION_ERROR 32


int checkVersion(uint8_t const * ptr, uint8_t size, uint8_t *error){
    if (size != 1 || *ptr != USERPASS_METHOD_VERSION) {
        *error = VERSION_ERROR;
        return false;
    }
    return true;
}

/**
 * auth_read_init
 * Initializes userpass_st variables
 * @param state
 * @param key
 */
void auth_read_init(unsigned state, struct selector_key *key){
    struct userpass_st *d = &ATTACHMENT(key)->client.userpass;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);

    int total_states = 5;
    d->status=AUTH_SUCCESS;

    d->parser = malloc(sizeof(*d->parser));
    if(d->parser == NULL) {
        d->status = AUTH_ERROR;
        return;
    }

    d->parser->size = total_states;

    d->parser->states = malloc(sizeof(parser_substate *) * total_states);
    if(d->parser->states == NULL) {
        d->status = AUTH_ERROR;
        return;
    }

    //// Read version
    d->parser->states[0] = malloc(sizeof(parser_substate));
    if(d->parser->states[0] == NULL) {
        d->status = AUTH_ERROR;
        return;
    }
    d->parser->states[0]->state = long_read;
    d->parser->states[0]->remaining = d->parser->states[0]->size = 1;
    d->parser->states[0]->result = malloc(sizeof(uint8_t) + 1);
    if(d->parser->states[0]->result == NULL) {
        d->status = AUTH_ERROR;
        return;
    }
    d->parser->states[0]->check_function = checkVersion;

    //// Nread for username
    d->parser->states[1] = malloc(sizeof(parser_substate));
    if(d->parser->states[1] == NULL) {
        d->status = AUTH_ERROR;
        return;
    }
    d->parser->states[1]->state = read_N;

    //// Read username
    d->parser->states[2] = malloc(sizeof(parser_substate));
    if(d->parser->states[2] == NULL) {
        d->status = AUTH_ERROR;
        return;
    }
    d->parser->states[2]->state = long_read;
    d->parser->states[2]->result = NULL;
    d->parser->states[2]->check_function = NULL;

    //// Nread for username
    d->parser->states[3] = malloc(sizeof(parser_substate));
    if(d->parser->states[3] == NULL) {
        d->status = AUTH_ERROR;
        return;
    }
    d->parser->states[3]->state = read_N;

    //// Read password
    d->parser->states[4] = malloc(sizeof(parser_substate));
    if(d->parser->states[4] == NULL) {
        d->status = AUTH_ERROR;
        return;
    }
    d->parser->states[4]->state = long_read;
    d->parser->states[4]->result = NULL;
    d->parser->states[4]->check_function = NULL;

    parser_init(d->parser);
}


/**
 * auth_read
 * Reads and parses client input
 * @param key
 */
unsigned auth_read(struct selector_key *key){
    struct userpass_st *d = &ATTACHMENT(key)->client.userpass;
    if(d->status==AUTH_ERROR){
        return ATTACHMENT(key)->error_state;
    }
    unsigned ret = USERPASS_READ;
    bool error = false;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    ptr = buffer_write_ptr(d->rb, &count);
    n = recv(key->fd, ptr, count, 0);
    if (n > 0) {

        buffer_write_adv(d->rb, n);

        const enum parser_state st = consume(d->rb, d->parser, &error);

        if (is_done(st, 0)) {
            if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
                ret = auth_process(d, key);
            } else {
                ret = ATTACHMENT(key)->error_state;
            }
        }
    } else {
        ret = ATTACHMENT(key)->error_state;;
    }
    return error ? ATTACHMENT(key)->error_state : ret;
}

int auth_reply(buffer *b, const uint8_t result)
{
    size_t n;
    uint8_t *buf = buffer_write_ptr(b, &n);
    if (n < 2)
    {
        return -1;
    }
    buf[0] = USERPASS_METHOD_VERSION;
    buf[1] = result;
    buffer_write_adv(b, 2);
    return 2;
}

/**
 * auth_read_close
 * Close resources
 * @param state
 * @param key
 */
void auth_read_close(unsigned state, struct selector_key *key){
    struct parser *p = ATTACHMENT(key)->client.userpass.parser;
    if(p != NULL) {
        if (p->states != NULL) {
            for (int i = 0; i < p->size; ++i) {
                if (p->states[i] != NULL) {
                    if (p->states[i]->state == long_read && p->states[i]->result != NULL) {
                        free(p->states[i]->result);
                    }
                    free(p->states[i]);
                }
            }
            free(p->states);
        }
        free(p);
    }
}


/**
 * auth_write_init
 * Initializes userpass_st variables
 * @param state
 * @param key
 */
void auth_write_init(unsigned state, struct selector_key *key){
    struct userpass_st *d = &ATTACHMENT(key)->client.userpass;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);
}


/**
 * auth_write
 * Checks authentication and wirtes answer to client
 * @param key
 */
unsigned auth_write(struct selector_key *key){
    struct userpass_st *d = &ATTACHMENT(key)->client.userpass;
    struct socks5 * data = ATTACHMENT(key);
    unsigned ret = USERPASS_WRITE;
    uint8_t *ptr;
    size_t count;
    ssize_t n;


    auth_reply(d->wb, ATTACHMENT(key)->authentication);
    ptr = buffer_read_ptr(d->wb, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);
    if (n == -1) {
        ret = ATTACHMENT(key)->error_state;
    } else {
        buffer_read_adv(d->wb, n);
        if (!buffer_can_read(d->wb)) {
            if(data->authentication != 0x00){
                return data->done_state;
            }
            if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_READ)) {
                ret = REQUEST_READ;
            } else {
                ret = ATTACHMENT(key)->error_state;
            }
        }
    }
    return ret;
}


/**
 * auth_write_close
 * Close resources
 * @param state
 * @param key
 */
void auth_write_close(unsigned state, struct selector_key *key){
}

/**
 * Auxiliar function for socks auth_process
 * @param username
 * @param password
 * @return
 */
uint8_t checkCredentials(uint8_t *username, uint8_t *password, struct selector_key * key) {
    for (int i = 0; i < nusers; ++i) {
        if (strcmp((char *) username, users[i].name) == 0) {
            if (strcmp((char *) password, users[i].pass) == 0){
                ATTACHMENT(key)->userIndex = i;
                return 0x00;
            }
        }
    }
    return 0x01;
}

/**
 * Auxiliar function for mng auth_process
 * @param username
 * @param password
 * @return
 */
uint8_t checkMngCredentials(uint8_t *username, uint8_t *password) {
    for (int i = 0; i < nadmins; ++i) {
        if (strcmp((char *) username, admins[i].name) == 0) {
            if (strcmp((char *) password, admins[i].pass) == 0)
                return 0x00;
        }
    }
    return 0x01;
}


/**
 *
 * @param d Userpass State (Contains client input credentials)
 * @param data
 * @return
 */
extern size_t metrics_historic_auth_attempts;
int auth_process(struct userpass_st *d, struct selector_key * key){
    struct socks5 * data = key->data;
    uint8_t *username = d->parser->states[2]->result;
    uint8_t *password = d->parser->states[4]->result;

    if(data->isSocks)
        data->authentication = checkCredentials(username, password, key);
    else
        data->authentication = checkMngCredentials(username, password);

    metrics_historic_auth_attempts += 1;    

    return USERPASS_WRITE;
}