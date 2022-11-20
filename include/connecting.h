#ifndef CONNECTING_H
#define CONNECTING_H
#include "selector.h"
#include "debug.h"
#include "socks5nio.h"
#include "stm.h"
#include "request.h"
#include "address_utils.h"
#include <netinet/in.h>

/**
 * @param state
 * @param key
 */
void connecting_init(const unsigned state, struct selector_key *key);
/**
 * @param key
 * @return
 */
unsigned connecting_write(struct selector_key *key);
/**
 * @param key
 * @return
 */
void connecting_close(const unsigned state, struct selector_key *key);

#endif