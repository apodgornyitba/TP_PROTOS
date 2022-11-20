#ifndef COPY_H
#define COPY_H

#include "selector.h"
#include "socks5nio.h"
#include "debug.h"
#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>

/* Función principal del PROXY
 * Hace el pasa manos de información entre dos sockets.
 * Cuando termina pasa el estado DONE. */
unsigned copy_write(struct selector_key *key);
unsigned copy_read(struct selector_key *key);
void copy_init(const unsigned int state, struct selector_key *key);

#endif