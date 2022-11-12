/* Codigo provisto por la cátedra */

#ifndef ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8
#define ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8

#include <stdbool.h>

#define MAX_USERS 10

struct users {
    char * name;
    char * pass;
};

struct doh {
    char * host;
    char * ip;
    unsigned short port;
    char * path;
    char * query;
};

struct socks5args {
    char * socks_addr;
    char * socks_addr_6;
    unsigned short socks_port;

    char * mng_addr;
    char * mng_addr_6;
    unsigned short mng_port;

    bool disectors_enabled;

    struct doh doh;
    struct users users[MAX_USERS];
};

/**
 * Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana. Puede cortar
 * la ejecución.
 */
void parse_args(const int argc, char **argv, struct socks5args *args);

#endif
