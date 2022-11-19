#ifndef CONNECTING_H
#define CONNECTING_H
#include "selector.h"
#include "debug.h"
#include "socks5nio.h"
#include "stm.h"
//#include "states.h"
/**
 * Etapa de conexión
 * Actualmente se conecta a un destino fijo.
 * Cuando termina pasa al estado COPY.
 */
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
unsigned connecting_read(struct selector_key *key);
#endif