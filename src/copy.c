#include "../include/copy.h"
#include <stdlib.h>
#include "../include/password_parser.h"

fd_interest copy_compute_interests(fd_selector s, struct copy_st *d)
{
    fd_interest ret = OP_NOOP;

    if(d->fd != -1)
    {
        if (((d->interest & OP_READ) && buffer_can_write(d->rb)) )
        {
            ret |= OP_READ;
        }
        if ((d->interest & OP_WRITE) && buffer_can_read(d->wb) )
        {
            ret |= OP_WRITE;
        }
        if (SELECTOR_SUCCESS != selector_set_interest(s, d->fd, ret))
        {
            abort();
        }
    }

    return ret;
}

extern uint8_t password_dissectors;

void copy_init(const unsigned int state, struct selector_key *key) {

    struct copy_st *d = &ATTACHMENT(key)->client.copy;
    d->fd = ATTACHMENT(key)->client_fd;
    d->rb = &ATTACHMENT(key)->read_buffer;
    d->wb = &ATTACHMENT(key)->write_buffer;
    d->interest = OP_READ | OP_WRITE;
    d->other_copy = &ATTACHMENT(key)->orig.copy;

    d = &ATTACHMENT(key)->orig.copy;
    d->fd = ATTACHMENT(key)->origin_fd;
    d->rb = &ATTACHMENT(key)->write_buffer;
    d->wb = &ATTACHMENT(key)->read_buffer;
    d->interest = OP_READ | OP_WRITE;
    d->other_copy = &ATTACHMENT(key)->client.copy;

    copy_compute_interests(key->s, d);
    copy_compute_interests(key->s, d->other_copy);
}

void * copy_ptr(struct selector_key *key){
    int current_fd = key->fd;
    struct socks5 * data = key->data;
    if(current_fd ==  data->origin_fd){
        return &data->orig.copy;
    }
    if(current_fd ==  data->client_fd){
        return &data->client.copy;
    }
    return NULL;
}

// lee bytes de un socket y los encola para ser escritos en otro socket
extern size_t metrics_historic_byte_transfer;
extern size_t metrics_average_bytes_per_read;
extern size_t total_reads;
extern size_t metrics_concurrent_connections;
unsigned copy_read(struct selector_key *key) {
    struct copy_st *d = copy_ptr(key);
    if(d == NULL){
        exit(EXIT_FAILURE);
    }
    assert(d->fd == key->fd);
    size_t size;
    ssize_t n;
    buffer *b = d->rb;
    unsigned ret = COPY;

    uint8_t *ptr = buffer_write_ptr(b, &size);
    n = recv(key->fd, ptr, size, 0);

    if (n <= 0)
    {
        // Si error o EOF cierro el canal de lectura y el canal de escritura del origin
        shutdown(d->fd, SHUT_RD);
        d->interest &= ~OP_READ;
        if (d->other_copy->fd != -1)
        {
            shutdown(d->other_copy->fd, SHUT_WR);
            d->other_copy->interest &= ~OP_WRITE;
        }
    }
    else
    {
        if(password_dissectors == 0x00)
            password_consume(ptr, n, &ATTACHMENT(key)->password_parser);

        if (total_reads == 0) {
            metrics_average_bytes_per_read = n;
            total_reads++;
        } else {
            metrics_average_bytes_per_read = ((metrics_average_bytes_per_read * total_reads) + n) / (total_reads + 1);
            total_reads++;
        }
        metrics_historic_byte_transfer += n;
        buffer_write_adv(b, n);
    }
    copy_compute_interests(key->s, d);
    copy_compute_interests(key->s, d->other_copy);
    if (d->interest == OP_NOOP)
    {
        metrics_concurrent_connections -= 1;
        ret = DONE;
    }

    return ret;
}

// escribe bytes encolados
extern size_t metrics_average_bytes_per_write;
extern size_t total_writes;
unsigned copy_write(struct selector_key *key) {
    struct copy_st *d = copy_ptr(key);

    assert(d->fd == key->fd);
    size_t size;
    ssize_t n;
    buffer *b = d->wb;
    unsigned ret = COPY;

    uint8_t *ptr = buffer_read_ptr(b, &size);
    n = send(key->fd, ptr, size, MSG_NOSIGNAL);

    if (n == -1)
    {
        shutdown(d->fd, SHUT_WR);
        d->interest &= ~OP_WRITE;
        if (d->other_copy->fd != -1)
        {
            shutdown(d->other_copy->fd, SHUT_RD);
            d->other_copy->interest &= ~OP_READ;
        }
    }
    else
    {
        //// Add written bytes to metrics
        metrics_historic_byte_transfer += n;
        if (total_writes == 0) {
            metrics_average_bytes_per_write = n;
            total_writes++;
        } else {
            metrics_average_bytes_per_write = ((metrics_average_bytes_per_write * total_writes) + n) / (total_writes + 1);
            total_writes++;
        }
        buffer_read_adv(b, n);
    }
    copy_compute_interests(key->s, d);
    copy_compute_interests(key->s, d->other_copy);
    if (d->interest == OP_NOOP)
    {
        metrics_concurrent_connections -= 1;
        ret = DONE;
    }

    return ret;
}