#include "../include/hello.h"

/*https://www.rfc-editor.org/rfc/rfc1928*/
void hello_parser_init(struct hello_parser *p) {
    p->state = hello_version;
    p->remaining = 0;
}

enum hello_state hello_parser_feed(struct hello_parser *p, uint8_t b)
{
    switch (p->state)
    {
        case hello_version:
            if (b == p->version)
            {
                p->state = hello_nmethods;
            }
            else
            {
                p->state = hello_error_unsupported_version;
            }
            break;
        case hello_nmethods:
            p->remaining = b;
            p->state = hello_methods;
            if (p->remaining <= 0)
            {
                p->state = hello_done;
            }
            break;
        case hello_methods:
            if (p->on_authentication_method != NULL)
            {
                p->on_authentication_method(p, b);
            }
            p->remaining--;
            if (p->remaining <= 0)
            {
                p->state = hello_done;
            }
            break;
        case hello_done:

            break;
        case hello_error_unsupported_version:

            break;
        default:
            abort();
            break;
    }
    return p->state;
}

bool hello_is_done(const enum hello_state state, bool *error)
{
    bool ret = false;
    switch (state)
    {
        case hello_error_unsupported_version:
            if (error != 0)
            {
                *error = true;
            }
            ret = true;
            break;
        case hello_done:
            ret = true;
            break;
        default:
            ret = false;
            break;
    }
    return ret;
}

enum hello_state hello_consume(buffer *b, struct hello_parser *p, bool *error)
{
    enum hello_state st = p->state;
    bool finished = false;
    while (buffer_can_read(b) && !finished)
    {
        uint8_t byte = buffer_read(b);
        st = hello_parser_feed(p, byte);
        if (hello_is_done(st, error))
        {
            finished = true;
        }
    }
    return st;
}

int hello_marshal(buffer *b, const uint8_t method, uint8_t version)
{
    size_t n;
    uint8_t *buf = buffer_write_ptr(b, &n);
    if (n < 2)
    {
        return -1;
    }
    buf[0] = version;
    buf[1] = method;
    buffer_write_adv(b, 2);
    return 2;
}

extern uint8_t auth_method;
/** callback del parser utilizado en `read_hello' */
static void on_hello_method(void *p, const uint8_t method) {
    struct hello_parser * parser = p;
    if(parser->method == method) {
        *((uint8_t *)(parser->data)) = method;
    }
}

/** Initialize the variables belonging to the HELLO state */
void hello_read_init(const unsigned state, struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->client.hello;
    d->rb = &(ATTACHMENT(key)->read_buffer);
    d->wb = &(ATTACHMENT(key)->write_buffer);
    d->method = 0xFF;
    d->parser = malloc(sizeof(*(d->parser)));
    d->status=HELLO_SUCCESS;
    if(d->parser==NULL){
        d->status=HELLO_ERROR;
        return;
    }
    d->parser->data = &(d->method);
    d->parser->on_authentication_method = on_hello_method;
    if(ATTACHMENT(key)->isSocks) {
        d->parser->version = SOCKS_VERSION;
        d->parser->method = auth_method;
    }
    else {
        d->parser->version = MNG_VERSION;
        d->parser->method = MNG_AUTH_METHOD;
    }    
    hello_parser_init(d->parser);
}

/** inicializa las variables de los estados HELLO_st */
void hello_write_init(const unsigned state, struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->client.hello;
    d->rb                              = &(ATTACHMENT(key)->read_buffer);
    d->wb                              = &(ATTACHMENT(key)->write_buffer);
}

/** lee todos los bytes del mensaje de tipo 'hello' y inicia su proceso */
unsigned hello_read(struct selector_key *key) {
    struct hello_st *d = &ATTACHMENT(key)->client.hello;
    if(d->status==HELLO_ERROR){
        return ATTACHMENT(key)->error_state;
    }
    unsigned  ret      = HELLO_READ;
    bool  error    = false;
    uint8_t *ptr;
    size_t  count;
    ssize_t  n;

    ptr = buffer_write_ptr(d->rb, &count);
    n = recv(key->fd, ptr, count, 0);
    if(n > 0) {
        buffer_write_adv(d->rb, n);
        const enum hello_state st = hello_consume(d->rb, d->parser, &error);
        if(hello_is_done(st, 0)) {
            if(SELECTOR_SUCCESS == selector_set_interest_key(key, OP_WRITE)) {
                int result;
                if(ATTACHMENT(key)->isSocks)
                    result = hello_process(d, SOCKS_VERSION);
                else result = hello_process(d, MNG_VERSION);
                if(result == -1)
                    ret = ATTACHMENT(key)->error_state;
                else ret = result;
            } else {
                ret = ATTACHMENT(key)->error_state;
            }
        }
    } else {
                ret = ATTACHMENT(key)->error_state;
    }
    return error ? ATTACHMENT(key)->error_state : ret;
}

/** procesamiento del mensaje `hello' */
static int hello_process(const struct hello_st* d, uint8_t version) {
    unsigned ret = HELLO_WRITE;

    uint8_t m = d->method;
    if (-1 == hello_marshal(d->wb, m, version)) {
        ret  = -1;
    }
    return ret;
}

void hello_read_close(const unsigned state, struct selector_key *key)
{
    struct hello_st *d = &ATTACHMENT(key)->client.hello;
    if(d->parser!=NULL){
        hello_parser_close(d->parser);
        free(d->parser);
    }
}

void hello_write_close(const unsigned state, struct selector_key *key)
{
}

unsigned hello_write(struct selector_key *key)
{
    struct hello_st *d = &ATTACHMENT(key)->client.hello;

    unsigned ret = HELLO_WRITE;
    uint8_t *ptr;
    size_t count;
    ssize_t n;


    ptr = buffer_read_ptr(d->wb, &count);
    n= send(key->fd, ptr, count, MSG_NOSIGNAL);

    if(n==-1){
        return ATTACHMENT(key)->error_state;
    }

    buffer_read_adv(d->wb,n);
    if(!buffer_can_read(d->wb)){
        if(d->method == METHOD_NO_ACCEPTABLE_METHODS){
            return ATTACHMENT(key)->done_state;
        }
        if(SELECTOR_SUCCESS== selector_set_interest_key(key, OP_READ)){
            if(d->method == METHOD_NO_AUTHENTICATION_REQUIRED)
                ret = REQUEST_READ;
            if(d->method == METHOD_USERNAME_PASSWORD)
                ret= USERPASS_READ;
        }else{
            ret=ATTACHMENT(key)->error_state;
        }
    }

    return ret;
}

void hello_parser_close(struct hello_parser *p){
}