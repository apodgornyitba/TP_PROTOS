#ifndef COPY_H
#define COPY_H

#include "selector.h"
#include "socks5nio.h"
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>

unsigned copy_write(struct selector_key *key);
unsigned copy_read(struct selector_key *key);
void copy_init(const unsigned int state, struct selector_key *key);
fd_interest copy_compute_interests(fd_selector s, struct copy_st *d);

#endif