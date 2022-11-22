#ifndef ADDRESS_UTILS_H
#define ADDRESS_UTILS_H

#include <sys/types.h>
#include <sys/socket.h>
#include "socks5nio.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/**
 * Converts an IPv4 or IPV6 address from text to binary (network) format with inet_pton.
 * @param address
 * @param addr
 * @param addr6
 * @return
 *          AF_INET if address is IPv4
 *          AF_INET6 if address is IPv6
 *          -1 if it does not match an IP address
**/
int address_processing(char * address, struct sockaddr_in * addr, struct sockaddr_in6 * addr6, uint16_t port);

#endif