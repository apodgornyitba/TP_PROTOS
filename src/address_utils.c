#include "../include/address_utils.h"

// https://stackoverflow.com/questions/3736335/tell-whether-a-text-string-is-an-ipv6-address-or-ipv4-address-using-standard-c-s 
int address_processing(char * address, struct sockaddr_in * addr, struct sockaddr_in6 * addr6, uint16_t port) {
    struct addrinfo hint, *res = NULL;
    int ret;
    memset(&hint, '\0', sizeof hint);

    hint.ai_family = PF_UNSPEC;
    hint.ai_flags = AI_NUMERICHOST;

    ret = getaddrinfo(address, NULL, &hint, &res);
    if(ret) {
        printf("Invalid address");
        return -1;
    }
    // IPv4
    if(res->ai_family == AF_INET) {
        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = inet_addr(address);
        addr->sin_port = htons(port);
        return AF_INET;
    }
    // IPv6
    else if (res->ai_family == AF_INET6) {
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(port);
        return AF_INET6;
    } else {
        printf("%s is an unknown address format %d\n",address,res->ai_family);
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

void set_addr(struct selector_key * key, struct addrinfo * current){
    struct socks5 * data = ATTACHMENT(key);

    if(current->ai_family == AF_INET) {
        data->origin_domain = AF_INET;
    }

    if(current->ai_family == AF_INET6) {
        data->origin_domain = AF_INET6;
    }

    data->origin_addr_len = current->ai_addrlen;
    memcpy(&data->origin_addr, current->ai_addr, current->ai_addrlen);

}
