#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros

#define TRUE   1
#define FALSE  0
#define PORT 8888
#define MAX_CLIENTS 500
#define BUFF_SIZE 1025

int main(){


    int opt = TRUE;
    int master_socket , addrlen , new_socket , client_socket[MAX_CLIENTS], activity, i , valread , sd;
    int max_sd;
    struct sockaddr_in address;

    char buffer[BUFF_SIZE];  //data buffer of 1K

    //set of socket read file descriptors
    fd_set readfds;

    for (i = 0; i < MAX_CLIENTS; i++) {
        client_socket[i] = 0;
    }

    //create a master socket
    if((master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections
    if(setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    //define socket type
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Listener port %d \n", PORT);

    //try to specify maximum of 500 pending connections for the master socket
    if (listen(master_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    //accept the incoming connection
    addrlen = sizeof(address);
    puts("Waiting for connections ...");

    while(TRUE) {
        //clear the socket set
        FD_ZERO(&readfds);

        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        //add child sockets to set
        for (i = 0 ; i < MAX_CLIENTS; i++) {
            //socket descriptor
            sd = client_socket[i];

            //if valid socket descriptor then add to read list
            if(sd > 0)
                FD_SET(sd , &readfds);

            //highest file descriptor number, need it for the select function
            if(sd > max_sd)
                max_sd = sd;
        }

        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select(max_sd + 1 , &readfds , NULL , NULL , NULL);

        if ((activity < 0) && (errno!=EINTR)) {
            printf("Select failed");
        }

        //any activity on the master socket -> incoming connection
        if (FD_ISSET(master_socket, &readfds)) {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
                perror("Accept");
                exit(EXIT_FAILURE);
            }

            //inform user of socket number - used in send and receive commands
            printf("------------------------------------\n");
            printf("New Connection Established\n");
            printf("Socket fd: %d\n", new_socket);
            printf("IP address: %s\n", inet_ntoa(address.sin_addr));
            printf("Port number: %d\n", ntohs(address.sin_port));
            printf("------------------------------------\n");

            //add new socket to array of sockets
            for (i = 0; i < MAX_CLIENTS; i++) {
                //if position is empty
                if(client_socket[i] == 0) {
                    client_socket[i] = new_socket;
                    printf("Socket added as %d\n" , i);
                    break;
                }
            }
        }

        //else its some IO operation on some other socket
        for (i = 0; i < MAX_CLIENTS; i++) {
            sd = client_socket[i];

            if (FD_ISSET( sd , &readfds)) {
                //Check if it was for closing , and also read the incoming message
                if ((valread = read( sd , buffer, BUFF_SIZE-1)) == 0) {
                    //Somebody disconnected, get his details and print
                    getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                    printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

                    //Close the socket and mark as 0 in list for reuse
                    close(sd);
                    client_socket[i] = 0;
                }

                //Echo back the message that came in
                else {
                    buffer[valread] = '\0';
                    printf("Received: %s", buffer);
                    send(sd , buffer , strlen(buffer) , 0);
                    printf("Sent: %s", buffer);
                }
            }
        }
    }

    return 0;
}