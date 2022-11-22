/* Codigo provisto por la cátedra (Juan Codagnone) */

#include <stdio.h>
#include <string.h>   //strlen
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>   //close
#include <arpa/inet.h>    //close
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros

#define IP_FOR_REQUESTS "192.168.56.2"
#define PORT_FOR_REQUESTS "80"

#define TRUE   1
#define FALSE  0
#define PORT 8888
#define MAX_CLIENTS 500
#define BUFF_SIZE 1025

int main(){

    int opt = TRUE;
    int master_socket, new_socket, directed_new_socket , client_socket[MAX_CLIENTS], directed_client_socket[MAX_CLIENTS];
    int addrlen, activity, i , valread;
    int max_sd, sd, directed_sd;
    

    char buffer[BUFF_SIZE];  

    //set of socket descriptors
    fd_set readfds;

    for (i = 0; i < MAX_CLIENTS; i++) {
        client_socket[i] = 0;
        directed_client_socket[i] = 0;
    }

    //create the server socket (master)
    if((master_socket = socket(AF_INET , SOCK_STREAM , 0)) <= 0) {
        perror("ERROR: Unable to create master socket");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections
    if(setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
        perror("ERROR: Unable to set master socket");
        exit(EXIT_FAILURE);
    }

    //address for socket binding
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    //bind the socket to localhost port 8888
    if(bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("ERROR: Unable to bind master socket");
        exit(EXIT_FAILURE);
    }
    printf("Listening on port %d \n", PORT);

    //check whether socket is able to listen
    if(listen(master_socket, MAX_CLIENTS) < 0) {
        perror("ERROR: Unable to listen. Maximum client limit surpassed.");
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
        for (i = 0 ; i < MAX_CLIENTS ; i++) {

            //socket descriptors
            sd = client_socket[i];
            directed_sd = directed_client_socket[i];

            //if socket descriptor is valid, add to read list
            if(sd > 0)
                FD_SET( sd , &readfds);
            if(directed_sd > 0)
                FD_SET( directed_sd , &readfds);

            //save the highest file descriptor for select
            if(sd > max_sd)
                max_sd = sd;
            if(directed_sd > max_sd)
                max_sd = directed_sd;
        }

        //wait for an activity on one of the sockets , timeout is NULL , so wait indefinitely
        activity = select(max_sd + 1 , &readfds , NULL , NULL , NULL);

        if ((activity < 0) && (errno!=EINTR)) {
            printf("ERROR: Select");
        }

        //If something happened on the master socket , then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)) {
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
                perror("ERROR: Accept");
                exit(EXIT_FAILURE);
            }

            //inform user of socket number - used in send and receive commands
            printf("------------------------------------\n");
            printf("New Connection Established\n");
            printf("Socket fd: %d\n", new_socket);
            printf("IP address: %s\n", inet_ntoa(address.sin_addr));
            printf("Port number: %d\n", ntohs(address.sin_port));
            printf("------------------------------------\n");

            // Create socket to destination
            struct addrinfo hints, *res;

            //get host info, make socket and connect it
            memset(&hints, 0,sizeof hints);
            hints.ai_family=AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;

            if(getaddrinfo(IP_FOR_REQUESTS, PORT_FOR_REQUESTS, &hints, &res) != 0){
                printf( "getadrrinfo failed");
                exit(EXIT_FAILURE);
            }

            directed_new_socket = socket(res->ai_family,res->ai_socktype,res->ai_protocol);

            printf( "Connecting...\n");

            if(connect(directed_new_socket,res->ai_addr,res->ai_addrlen) != 0) {
                printf( "Connect to fixed destination failed.");
                exit(EXIT_FAILURE);
            }

            printf( "Connected!\n");

            //add new socket to array of sockets
            for (i = 0; i < MAX_CLIENTS; i++) {
                //if position is empty
                if(client_socket[i] == 0) {
                    directed_client_socket[i] = directed_new_socket;
                    client_socket[i] = new_socket;
                    printf("Adding to list of sockets as %d\n" , i);
                    break;
                }
            }
        }

        //else its some IO operation on some other socket :)
        for (i = 0; i < MAX_CLIENTS; i++) {
            sd = client_socket[i];
            directed_sd = directed_client_socket[i];

            if (FD_ISSET( sd , &readfds)) {
                //Check if it was for closing , and also read the incoming message
                if ((valread = read( sd , buffer, BUFF_SIZE-1)) == 0) {
                    //Somebody disconnected , get his details and print
                    getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                    printf("Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

                    //Close the socket and mark as 0 in list for reuse
                    close( sd );
                    client_socket[i] = 0;

                    //Somebody disconnected , get his details and print
                    getpeername(directed_sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                    printf( "Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                    //Close the socket and mark as 0 in list for reuse
                    close( directed_sd );
                    directed_client_socket[i] = 0;
                }

                    //Echo back the message that came in
                else {
                    //set the string terminating NULL byte on the end of the data read
                    buffer[valread] = '\0';
                    printf( "%s\n%s", "Received from client:", buffer);
                    send(directed_sd , buffer , strlen(buffer) , 0 );
                    printf( "%s\n%s", "Sent to destination:", buffer);
                }
            }
        }

        //else its some IO operation on some other socket
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            directed_sd = directed_client_socket[i];
            sd = client_socket[i];
            if (FD_ISSET( directed_sd , &readfds)) {
                //Check if it was for closing , and also read the incoming message
                if ((valread = read( directed_sd , buffer, BUFF_SIZE-1)) == 0) {
                    //Somebody disconnected , get his details and print
                    getpeername(directed_sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                    printf( "Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));
                    //Close the socket and mark as 0 in list for reuse
                    close( directed_sd );
                    directed_client_socket[i] = 0;

                    //Somebody disconnected , get his details and print
                    getpeername(sd , (struct sockaddr*)&address , (socklen_t*)&addrlen);
                    printf( "Host disconnected , ip %s , port %d \n" , inet_ntoa(address.sin_addr) , ntohs(address.sin_port));

                    //Close the socket and mark as 0 in list for reuse
                    close( sd );
                    client_socket[i] = 0;
                }

                    //Echo back the message that came in
                else {
                    //set the string terminating NULL byte on the end of the data read
                    buffer[valread] = '\0';
                    printf("Received from destination:\n%s", buffer);
                    send(sd , buffer , strlen(buffer) , 0 );
                    printf("Sent to client:\n%s", buffer);
                }
            }
        }
    }

    return 0;
}
