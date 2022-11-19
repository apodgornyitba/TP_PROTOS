#ifndef MANAGEMENT_REQUEST_H
#define MANAGEMENT_REQUEST_H

#include "selector.h"

void mng_request_index_init(unsigned state, struct selector_key *key);

unsigned mng_request_index_read(struct selector_key *key);

void mng_request_index_close(unsigned state, struct selector_key *key);

void mng_request_init(unsigned state, struct selector_key *key);

unsigned mng_request_read(struct selector_key *key);

void mng_request_close(unsigned state, struct selector_key *key);

void mng_request_write_init(unsigned state, struct selector_key * key);

unsigned mng_request_write(struct selector_key *key);

void mng_request_write_close(unsigned state, struct selector_key * key);

#endif