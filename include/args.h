/* Codigo provisto por la cátedra (Juan Codagnone) */
/* MODIFICADO */

#ifndef ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8
#define ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8

#include <stdbool.h>
#include <netinet/in.h>
#include "socks5nio.h"

extern struct users users[MAX_USERS];
extern int nusers;

struct socks5args {
    //server 
    char * socks_addr;
    char * socks_addr_6;
    unsigned short socks_port;
    int socks_family;
    struct sockaddr_in  socks_addr_info;
    struct sockaddr_in6 socks_addr_info_6;

    //mng
    char * mng_addr;
    char * mng_addr_6;
    unsigned short mng_port;
    int mng_family;
    struct sockaddr_in  mng_addr_info;
    struct sockaddr_in6 mng_addr_info_6;

    //buffer
    uint16_t buffer_size, mng_buffer_size;

    //server config
    uint8_t disectors_enabled;
    // uint8_t auth_enabled;

    //user config
    char * credentials;
};

/* Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana.
 * Devuelve -1 si hubo error o una de las opciones es solo de imprimir a STDOUT.
 * Devuelve 0 caso contrario */
int parse_args(const int argc, char * const * argv, struct socks5args * args);

#endif