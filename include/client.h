#ifndef CLIENT_H
#define CLIENT_H

#include <stdint.h>

int handshake(int sockfd, struct user* user);

uint8_t handshake_response(int sockfd);

#endif