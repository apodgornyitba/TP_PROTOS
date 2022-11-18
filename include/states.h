#ifndef STATES_H
#define STATES_H
#include "buffer.h"
#include "hello.h"
#include "request.h"

// Definición de variables para cada estado

/** Used by the HELLO_READ and HELLO_WRITE states */
typedef struct hello_st
{
    /** Buffers used for IO */
    buffer *rb, *wb;
    /** Pointer to hello parser */
    struct hello_parser *parser;
    /** Selected auth method */
    uint8_t method;
};

/** Used by the USERPASS_READ and USERPASS_WRITE states */
//typedef struct userpass_st
//{
/** Buffers used for IO */
//buffer *rb, *wb;
/** Pointer to hello parser */
//struct up_req_parser parser;
/** Selected user */
//uint8_t * user;
/** Selected password */
//uint8_t * password;
//};

/** Used by the REQUEST_READ, REQUEST_WRITE and REQUEST_RESOLV state */
enum socks5_response_status {
    HOLA
};
typedef struct request_st
{
    buffer *rb, *wb;


    struct request request;
    struct request_parser parser;


    enum socks5_response_status status;

    struct sockaddr_storage *origin_addr;
    socklen_t *origin_addr_len;
    int *origin_domain;

    const int *client_fd;
    int *origin_fd;
};

/** Used by REQUEST_CONNECTING */
typedef struct connecting{
    buffer *wb;
    const int *client_fd;
    int *origin_fd;
    enum socks_reply_status *status;
};

/** Used by the COPY state */
typedef struct copy_st
{
    /** File descriptor */
    int fd;
    /** Reading buffer */
    buffer * rb;
    /** Writing buffer */
    buffer * wb;
    /** Interest of the copy */
    fd_interest interest;
    /** Pointer to the structure of the opposing copy state*/
    struct copy_st * other_copy;
};

#endif