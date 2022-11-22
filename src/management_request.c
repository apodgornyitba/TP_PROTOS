#include <stdlib.h>
#include <string.h>
#include "../include/management.h"
#include "../include/states.h"
#include "../include/socks5nio.h"
#include "../include/management_request.h"

extern uint8_t auth_method;
#define METHOD_NO_AUTHENTICATION_REQUIRED 0x00
#define METHOD_USERNAME_PASSWORD 0x02
#define METHOD_NO_ACCEPTABLE_METHODS 0xFF

struct parser *mng_request_index_add_user_parser_init(struct selector_key *key);

struct parser *mng_request_index_delete_user_parser_init(struct selector_key *key);

struct parser *mng_request_index_yes_no_value(struct selector_key *key);

enum mng_state process_mng_index(struct selector_key *key, buffer *wb, enum mng_request_indexes index);

void status_mng_marshal(buffer *wb, enum mng_reply_status status);

void process_mng_params_request(struct selector_key *key, buffer *wb, enum mng_request_indexes index);

void list_users_mng_request_marshall(buffer *wb, struct selector_key *key);


int checkIndex(uint8_t const *ptr, uint8_t size, uint8_t *error) {
    switch (*ptr) {
        case mng_request_index_supported_indexes:
        case mng_request_index_list_users:
        case mng_request_index_historic_connections:
        case mng_request_index_concurrent_connections:
        case mng_request_index_max_concurrent_connections:
        case mng_request_index_historic_bytes_transferred:
        case mng_request_index_historic_auth_attempts:
        case mng_request_index_historic_connections_attempts:
        case mng_request_index_average_bytes_per_read:
        case mng_request_index_average_bytes_per_write:
        case mng_request_index_add_user:
        case mng_request_index_delete_user:
        case mng_request_index_disable_auth:
        case mng_request_index_disable_password_disectors:
            return true;
        default: {
            *error = mng_status_index_not_supported;
            return false;
        }
    }
}

void mng_request_index_init(const unsigned state, struct selector_key *key) {
    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);

    int total_states = 1;
    d->status=mng_status_succeeded;

    d->parser = malloc(sizeof(*d->parser));
    if(d->parser==NULL){
        d->status=mng_status_server_error;
        return;
    }

    d->parser->size = total_states;

    d->parser->states = malloc(sizeof(parser_substate *) * total_states);
     if(d->parser->states==NULL){
        d->status=mng_status_server_error;
        return;
    }

    // Read index
    d->parser->states[0] = malloc(sizeof(parser_substate));
     if(d->parser->states[0]==NULL){
        d->status=mng_status_server_error;
        return;
    }
    d->parser->states[0]->state = long_read;
    d->parser->states[0]->remaining = d->parser->states[0]->size = 1;
    d->parser->states[0]->result = malloc(sizeof(uint8_t) + 1);
    if(d->parser->states[0]->result==NULL){
        d->status=mng_status_server_error;
        return;
    }
    d->parser->states[0]->check_function = checkIndex;

    parser_init(d->parser);

}

unsigned mng_request_index_read(struct selector_key *key) {

    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;

    if(d->status!=mng_status_succeeded){
        status_mng_marshal(d->wb, d->status);
        if (SELECTOR_SUCCESS != selector_set_interest_key(key, OP_WRITE))
            return MNG_ERROR;
        return MNG_REQUEST_WRITE;
    }

    unsigned ret = MNG_REQUEST_READ_INDEX;
    bool error = false;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    ptr = buffer_write_ptr(d->rb, &count);
    if (count <= 0){
        d->status=mng_status_server_error;
        status_mng_marshal(d->wb, d->status);
        if (SELECTOR_SUCCESS != selector_set_interest_key(key, OP_WRITE))
            return MNG_ERROR;
        return MNG_REQUEST_WRITE;
    }
    // Only read one to check the index
    n = recv(key->fd, ptr, 1, 0);       
    if (n > 0) {

        buffer_write_adv(d->rb, n);

        const enum parser_state st = consume(d->rb, d->parser, &error);

        if (is_done(st, 0)) {
            if (error) {
                d->status = mng_status_index_not_supported;
                status_mng_marshal(d->wb, d->status);
                if (SELECTOR_SUCCESS != selector_set_interest_key(key, OP_WRITE))
                    return MNG_ERROR;
                ret = MNG_REQUEST_WRITE;
            } else {
                d->index = *d->parser->states[0]->result;
                ret = process_mng_index(key, d->wb, d->index);
                if (ret == MNG_REQUEST_WRITE) {
                    if (SELECTOR_SUCCESS != selector_set_interest_key(key, OP_WRITE))
                        return MNG_ERROR;
                }
                if (ret == MNG_REQUEST_READ) {
                    if (SELECTOR_SUCCESS != selector_set_interest_key(key, OP_READ))
                        return MNG_ERROR;
                }
            }
        }
    } else {
        ret = MNG_ERROR;
    }
    return ret;
}

void mng_request_index_close(const unsigned state, struct selector_key *key) {
    struct parser *p = ATTACHMENT(key)->client.mng_request.parser;
    if(p!=NULL) {
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

void mng_request_init(const unsigned state, struct selector_key *key) {
    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);

    d->status=mng_status_succeeded;

    switch (ATTACHMENT(key)->client.mng_request.index) {
        case mng_request_index_add_user: {
            if(mng_request_index_add_user_parser_init(key)==NULL)
                return;
            break;
        }
        case mng_request_index_delete_user: {
            if(mng_request_index_delete_user_parser_init(key)==NULL)
                return;
            break;
        }
        case mng_request_index_disable_auth:
        case mng_request_index_disable_password_disectors:
            if(mng_request_index_yes_no_value(key)==NULL)
                return;
            break;
        default: {
            abort();
        }
    }

}

unsigned mng_request_read(struct selector_key *key) {
    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;
    if(d->status!=mng_status_succeeded){
        status_mng_marshal(d->wb, d->status);
        if (SELECTOR_SUCCESS != selector_set_interest_key(key, OP_WRITE))
            return MNG_ERROR;
        return MNG_REQUEST_WRITE;
    }

    unsigned ret = MNG_REQUEST_READ;
    bool error = false;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    ptr = buffer_write_ptr(d->rb, &count);
    // Only read one to check the index
    n = recv(key->fd, ptr, count, 0);      
    if (n > 0) {
        buffer_write_adv(d->rb, n);
        const enum parser_state st = consume(d->rb, d->parser, &error);

        if (is_done(st, 0)) {
            if (SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
                if(error){
                    ret = MNG_ERROR;
                } else {
                    process_mng_params_request(key, d->wb, d->index);
                    ret = MNG_REQUEST_WRITE;
                }
            } else {
                ret = MNG_ERROR;
            }
        }
    } else {
        ret = MNG_ERROR;
    }
    return ret;
}

void mng_request_close(const unsigned state, struct selector_key *key) {
    struct parser *p = ATTACHMENT(key)->client.mng_request.parser;
     if(p!=NULL) {
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

void mng_request_write_init(unsigned state, struct selector_key *key) {
    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);
}

unsigned mng_request_write(struct selector_key *key) {
    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;

    unsigned ret = MNG_REQUEST_WRITE;
    uint8_t *ptr;
    size_t count;
    ssize_t n;

    ptr = buffer_read_ptr(d->wb, &count);
    n = send(key->fd, ptr, count, MSG_NOSIGNAL);

    if(n==-1){
        return MNG_ERROR;
    }

    buffer_read_adv(d->wb, n);
    // Check whether still capable to read
    if(!buffer_can_read(d->wb)){        
        return MNG_DONE;
    }

    return ret;
}

void mng_request_write_close(unsigned state, struct selector_key * key){
}

// Historic connections
extern size_t metrics_historic_connections;

// Concurrent connections
extern size_t metrics_concurrent_connections;

// Max concurrent connections
extern size_t metrics_max_concurrent_connections;

// Historic byte transfer
extern size_t metrics_historic_byte_transfer;

// Historic auth attempts
extern size_t metrics_historic_auth_attempts;

// Historic connections attempts
extern size_t metrics_historic_connections_attempts;

// Average bytes per read
extern size_t metrics_average_bytes_per_read;

// Average bytes per write
extern size_t metrics_average_bytes_per_write;

void uint_mng_request_marshall(buffer *wb, uint32_t value);

void supported_indexes_mng_request_marshall(buffer *wb);

enum mng_state process_mng_index(struct selector_key *key, buffer *wb, enum mng_request_indexes index) {
    switch (index) {
        case mng_request_index_supported_indexes: {                      // Supported indexes
            supported_indexes_mng_request_marshall(wb);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_list_users: {                             // List users
            list_users_mng_request_marshall(wb, key);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_historic_connections: {                   // Historic connections
            uint_mng_request_marshall(wb, metrics_historic_connections);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_concurrent_connections: {                 // Concurrent connections
            uint_mng_request_marshall(wb, metrics_concurrent_connections);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_max_concurrent_connections: {             // Max concurrent connections
            uint_mng_request_marshall(wb, metrics_max_concurrent_connections);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_historic_bytes_transferred: {             // Historic byte transfer
            uint_mng_request_marshall(wb, metrics_historic_byte_transfer);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_historic_auth_attempts: {                 // Historic auth attempts
            uint_mng_request_marshall(wb, metrics_historic_auth_attempts);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_historic_connections_attempts: {          // Historic connections attempts
            uint_mng_request_marshall(wb, metrics_historic_connections_attempts);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_average_bytes_per_read: {                 // Average bytes per read
            uint_mng_request_marshall(wb, metrics_average_bytes_per_read);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_average_bytes_per_write: {                // Average bytes per write
            uint_mng_request_marshall(wb, metrics_average_bytes_per_write);
            return MNG_REQUEST_WRITE;
        }
        case mng_request_index_add_user:                                 // Add user
        case mng_request_index_delete_user:                              // Delete user
        case mng_request_index_disable_auth:                             // Disable auth
        case mng_request_index_disable_password_disectors:               // Disable password dissectors
            return MNG_REQUEST_READ;

        default: {
            return MNG_ERROR;
        }
    }
}

extern struct users users[MAX_USERS];
extern int nusers;
void addUser(uint8_t * username, uint8_t * password){
    if(nusers >= MAX_USERS)
        return;
    char *newUsername = malloc(strlen((char *) username) + 1);
    char *newPassword = malloc(strlen((char *) password) + 1);
    strcpy(newUsername, (char *) username);
    strcpy(newPassword, (char *) password);
    users[nusers].name = newUsername;
    users[nusers++].pass = newPassword;
}

void deleteUser(uint8_t * username){
    bool deleted = false;
    for (int i = 0; i < nusers; ++i) {
        if(deleted){
            users[i-1].name = users[i].name;
            users[i-1].pass = users[i].pass;
        }
        if(!deleted && strcmp((const char *) username, users[i].name) == 0){
            free(users[i].name);
            free(users[i].pass);
            users[i].name = NULL;
            users[i].pass = NULL;
            deleted = true;
        }
    }
    if (deleted == true)
        nusers--;
}

void disableAuth(const uint8_t * option){
    if(*option == 0x00) {
        auth_method = METHOD_USERNAME_PASSWORD;
    } else {
        auth_method = METHOD_NO_AUTHENTICATION_REQUIRED;
    }
}
extern uint8_t password_dissectors;
void disablePasswordDissectors(const uint8_t * option){
    password_dissectors = *option;
}

void process_mng_params_request(struct selector_key *key, buffer *wb, enum mng_request_indexes index) {
    struct mng_request_st * st = &ATTACHMENT(key)->client.mng_request;

    switch (index) {
        case mng_request_index_add_user:{
            if(nusers == MAX_USERS)
                status_mng_marshal(wb, mng_status_max_users_reached);
            else{
                addUser(st->parser->states[1]->result, st->parser->states[3]->result);
                status_mng_marshal(wb, mng_status_succeeded);
            }
            break;
        }
        case mng_request_index_delete_user:{
            deleteUser(st->parser->states[1]->result);
            status_mng_marshal(wb, mng_status_succeeded);
            break;
        }
        case mng_request_index_disable_auth:{
            disableAuth(st->parser->states[0]->result);
            status_mng_marshal(wb, mng_status_succeeded);
            break;
        }
        case mng_request_index_disable_password_disectors:{
            disablePasswordDissectors(st->parser->states[0]->result);
            status_mng_marshal(wb, mng_status_succeeded);
            break;
        }
        case mng_request_index_supported_indexes:
        case mng_request_index_list_users:
        case mng_request_index_historic_connections:
        case mng_request_index_concurrent_connections:
        case mng_request_index_max_concurrent_connections:
        case mng_request_index_historic_bytes_transferred:
        case mng_request_index_historic_auth_attempts:
        case mng_request_index_historic_connections_attempts:
        case mng_request_index_average_bytes_per_read:
        case mng_request_index_average_bytes_per_write:
        default:
            status_mng_marshal(wb, mng_status_server_error);
            break;
    }
}

void uint_mng_request_marshall(buffer *wb, uint32_t value) {
    size_t n;
    uint8_t *buf = buffer_write_ptr(wb, &n);
    if (n < 5)
        return;
    buf[0] = 0x00;
    uint8_t *aux = (uint8_t *) (&(value));
    buf[1] = aux[0];
    buf[2] = aux[1];
    buf[3] = aux[2];
    buf[4] = aux[3];
    buffer_write_adv(wb, 5);
}

void supported_indexes_mng_request_marshall(buffer *wb) {
    size_t n;
    uint8_t *buf = buffer_write_ptr(wb, &n);
    if (n < 18)
        return;
    int i = 0;
    buf[i++] = 0x00;
    buf[i++] = 0x0E;
    buf[i++] = 0x00;
    buf[i++] = 0x01;
    buf[i++] = 0x02;
    buf[i++] = 0x03;
    buf[i++] = 0x04;
    buf[i++] = 0x05;
    buf[i++] = 0x06;
    buf[i++] = 0x07;
    buf[i++] = 0x08;
    buf[i++] = 0x09;
    buf[i++] = 0x0A;
    buf[i++] = 0x0B;
    buf[i++] = 0x0C;
    buf[i++] = 0x0D;

    buffer_write_adv(wb, i);
}

void list_users_mng_request_marshall(buffer *wb, struct selector_key *key) {
    size_t n;
    uint8_t *buf = buffer_write_ptr(wb, &n);
    size_t aux = n, auxUsers = nusers, iUsers = 0, lengthUser;
    ssize_t i = 0;
    size_t userLengths[nusers];
    //// Check si la info entra en el wb
    aux -= 2;                                         //// Status y nusers
    while (auxUsers != 0) {
        lengthUser = strlen(users[iUsers].name);    //// Users y su sizeq
        userLengths[iUsers++] = lengthUser;
        if (lengthUser > aux)
            return;
        auxUsers--;
        aux -= lengthUser;
    }

    //// Escribo la info en el buffer
    auxUsers = nusers;
    iUsers = 0;
    buf[i++] = 0x00;
    buf[i++] = nusers;
    char *user;
    while (auxUsers != 0) {
        buf[i++] = userLengths[iUsers];
        user = users[iUsers].name;
        memcpy((char *) (buf + i), user, userLengths[iUsers]);
        i += userLengths[iUsers++];
        auxUsers--;
    }
    buffer_write_adv(wb, i);
}

void status_mng_marshal(buffer *wb, enum mng_reply_status status) {
    size_t n;
    uint8_t *buf = buffer_write_ptr(wb, &n);
    if (n < 1)
        return;
    buf[0] = status;

    buffer_write_adv(wb, 1);
}

struct parser *mng_request_index_add_user_parser_init(struct selector_key *key) {
    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);

    int total_states = 4;

    d->parser = malloc(sizeof(*d->parser));
    if(d->parser==NULL){
        d->status=mng_status_server_error;
        return NULL;
    }

    d->parser->size = total_states;

    d->parser->states = malloc(sizeof(parser_substate *) * total_states);
    if(d->parser->states==NULL){
        d->status=mng_status_server_error;
        return NULL;
    }

    //// Nread for username
    d->parser->states[0] = malloc(sizeof(parser_substate));
    if(d->parser->states[0]==NULL){
        d->status=mng_status_server_error;
        return NULL;
    }
    d->parser->states[0]->state = read_N;

    //// Read username
    d->parser->states[1] = malloc(sizeof(parser_substate));
    if(d->parser->states[1]==NULL){
        d->status=mng_status_server_error;
        return NULL;
    }
    d->parser->states[1]->state = long_read;
    d->parser->states[1]->result = NULL;
    d->parser->states[1]->check_function = NULL;

    //// Nread for password
    d->parser->states[2] = malloc(sizeof(parser_substate));
    if(d->parser->states[2]==NULL){
        d->status=mng_status_server_error;
        return NULL;
    }
    d->parser->states[2]->state = read_N;

    //// Read password
    d->parser->states[3] = malloc(sizeof(parser_substate));
    if(d->parser->states[3]==NULL){
        d->status=mng_status_server_error;
        return NULL;
    }
    d->parser->states[3]->state = long_read;
    d->parser->states[3]->result = NULL;
    d->parser->states[3]->check_function = NULL;

    parser_init(d->parser);

    return d->parser;
}

struct parser *mng_request_index_delete_user_parser_init(struct selector_key *key) {
    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);

    int total_states = 2;

    d->parser = malloc(sizeof(*d->parser));
    if(d->parser==NULL){
        d->status=mng_status_server_error;
        return NULL;
    }

    d->parser->size = total_states;

    d->parser->states = malloc(sizeof(parser_substate *) * total_states);
    if(d->parser->states==NULL){
        d->status=mng_status_server_error;
        return NULL;
    }

    //// Nread for username
    d->parser->states[0] = malloc(sizeof(parser_substate));
    if(d->parser->states[0]==NULL){
        d->status=mng_status_server_error;
        return NULL;
    }
    d->parser->states[0]->state = read_N;

    //// Read username
    d->parser->states[1] = malloc(sizeof(parser_substate));
    if(d->parser->states[1]==NULL){
        d->status=mng_status_server_error;
        return NULL;
    }
    d->parser->states[1]->state = long_read;
    d->parser->states[1]->result = NULL;
    d->parser->states[1]->check_function = NULL;

    parser_init(d->parser);

    return d->parser;
}

struct parser *mng_request_index_yes_no_value(struct selector_key *key) {
    struct mng_request_st *d = &ATTACHMENT(key)->client.mng_request;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);

    int total_states = 1;

    d->parser = malloc(sizeof(*d->parser));
    if(d->parser==NULL){
        d->status=mng_status_server_error;
        return NULL;
    }

    d->parser->size = total_states;

    d->parser->states = malloc(sizeof(parser_substate *) * total_states);
    if(d->parser->states==NULL){
        d->status=mng_status_server_error;
        return NULL;
    }

    //// Read version
    d->parser->states[0] = malloc(sizeof(parser_substate));
    if(d->parser->states[0]==NULL){
        d->status=mng_status_server_error;
        return NULL;
    }
    d->parser->states[0]->state = long_read;
    d->parser->states[0]->remaining = d->parser->states[0]->size = 1;
    d->parser->states[0]->result = malloc(sizeof(uint8_t) + 1);
    if(d->parser->states[0]->result==NULL){
        d->status=mng_status_server_error;
        return NULL;
    }
    d->parser->states[0]->result[1] = 0;
    d->parser->states[0]->check_function = NULL;

    parser_init(d->parser);

    return d->parser;
}