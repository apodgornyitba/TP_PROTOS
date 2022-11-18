#include "../include/address_utils.h"
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>


int address_processing(char * address, struct sockaddr_in * addr, struct sockaddr_in6 * addr6, uint16_t port) {
    struct addrinfo hint, *res = NULL;
    int ret;
    memset(&hint, '\0', sizeof hint);

    hint.ai_family = PF_UNSPEC;
    hint.ai_flags = AI_NUMERICHOST;

    ret = getaddrinfo(address, NULL, &hint, &res);
    if(ret) {
        puts("Invalid address");
        return -1;
    }
    if(res->ai_family == AF_INET) {
        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = inet_addr(address);
        addr->sin_port = htons(port);
        return AF_INET;
    } else if (res->ai_family == AF_INET6) {
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(port);
        return AF_INET6;
    } else {
        printf("%s is an is unknown address format %d\n",address,res->ai_family);
    }

   freeaddrinfo(res);
   return 0;
    
    // // IPv4
    // int result = inet_pton(AF_INET, address, &addr->sin_addr);

    // // If address is not IPv4, then it must be IPv6
    // if (result <= 0) {
    //     // IPv6
    //     result = inet_pton(AF_INET6, address, &addr6->sin6_addr);

    //     if(result <= 0)
    //         return -1;
    //     else {
    //         addr6->sin6_family = AF_INET6;
    //         addr6->sin6_port = htons(port);
    //         return AF_INET6;
    //     }
    // }
    // else {
    //     
    // }
}