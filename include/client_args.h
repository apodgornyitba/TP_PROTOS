#ifndef CLIENT_ARGS_H
#define CLIENT_ARGS_H

#include <stdbool.h>
#include <netinet/in.h>

struct user{
    char* username;
    char* password;
    bool credentials;
};
struct management_args{
    char               *mng_addr;
    char               *mng_addr_6;
    unsigned short      mng_port;
    int                 mng_family;
    struct sockaddr_in  mng_addr_info;
    struct sockaddr_in6 mng_addr_info_6;

    struct user* user;

    char * file_path;
    bool append;

    bool debug;
};

int parse_args(const int argc, char *const * argv, struct management_args *args);

static int port(const char *s);
#endif