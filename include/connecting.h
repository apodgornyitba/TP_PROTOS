#ifndef CONNECTING_H
#define CONNECTING_H

#include "selector.h"
#include "debug.h"
#include "socks5nio.h"
#include "stm.h"
#include "request.h"
#include "address_utils.h"
#include <netinet/in.h>

void connecting_init(const unsigned state, struct selector_key *key);
/* Se fija si se completo la conexión */
unsigned connecting_write(struct selector_key *key);
void connecting_close(const unsigned state, struct selector_key *key);

#endif