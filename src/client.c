#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <errno.h>
#include "../include/client_args.h"
#include "../include/client_utils.h"
#include "../include/client.h"

int handshake(int sockfd, struct user* user){
    uint8_t buffer[4];
    int bytes_to_send;
    buffer[0]=0x01; //VERSION
    if(user->credentials){
        buffer[1]=0x02; //NMETODS
        buffer[2]=0x00; //NO AUTHENTICATION REQUIRED
        buffer[3]=0x02; //USERNAME/PASSWORD
        bytes_to_send=4;
    }else{
        buffer[1]=0x01; //NMETODS
        buffer[2]=0x00; //NO AUTHENTICATION REQUIRED
        bytes_to_send=3;
    }
    return send(sockfd, buffer, bytes_to_send,0);
}

uint8_t handshake_response(int sockfd){
    uint8_t buff[2];
    int rec=recv(sockfd, buff, 2, 0);
    if(rec != 2) {
        fprintf(stderr,"Error: handshake_response failed\n");
        return 0xFF;
    }
    return buff[1];
}

FILE * append_file;
int main(const int argc, const char **argv)
{
    struct management_args * args = malloc(sizeof(struct management_args));
    if(args == NULL) {
        exit(EXIT_FAILURE);
    }
    memset(args, 0, sizeof(*args));
    args->user= malloc(sizeof(struct user));
    if(args->user == NULL) {
        exit(EXIT_FAILURE);
    }

    int parse_args_result = parse_args(argc, (char *const *)argv, args);

    if(parse_args_result == -1){
        free(args);
        exit(1);
    }

    if(args->append){
        append_file = fopen(args->file_path, "a");
        if(append_file == NULL){
            fprintf(stderr,"Error opening %s\n", args->file_path);
            return -1;
        }
    } else append_file = NULL;


    int sockfd;

    switch (args->mng_family) {
        case AF_UNSPEC:{
            sockfd= socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
            if (connect(sockfd,(const struct sockaddr*) &args->mng_addr_info, sizeof(struct sockaddr)) < 0) {
                goto end;
            }
            break;
        }
        case AF_INET:{
            sockfd= socket(AF_INET, SOCK_STREAM,IPPROTO_TCP);
            if (connect(sockfd,(const struct sockaddr*) &args->mng_addr_info, sizeof(struct sockaddr)) < 0) {
                goto end;
            }
            break;
        }
        case AF_INET6:{
            sockfd= socket(AF_INET6, SOCK_STREAM,IPPROTO_TCP);
            if (sockfd < 0) {
            }
            if (connect(sockfd,(const struct sockaddr*) &args->mng_addr_info_6, sizeof(struct sockaddr_in6)) < 0) {
                goto end;
            }
            break;
        }    
    }
    int ret=handshake(sockfd, args->user);
    if(ret < 0){
        printf("Error in sending handshake\n");
        goto end;
    }


    uint8_t response=handshake_response(sockfd);
    if(response==0xFF) {
        printf("Error in handshake response\n");
        goto end;
    }

    if(response==0x02) {
            if(send_credentials(sockfd, args->user)< 0) {
            printf("Error sending credentials\n");
            goto end;
        }
    }

    response=credentials_response(sockfd);
    if(response!=0x00) {
        printf("Error on user authentication.\n");
        goto end;
    }

    int req_index;
    ret= send_request(sockfd, &req_index);
    if(ret < 0)
        goto end;

    if(request_response(sockfd, req_index) <0)
        goto end;

    end:
    if(sockfd >= 0)
        close(sockfd);
    free(args->user);
    free(args);

    return 0;
}
