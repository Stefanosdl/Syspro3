#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <signal.h>
#include "./headers/client.h"
#include "./headers/list.h"

struct Node* head = NULL;

void sigintServer(int sig){
  deleteList(&head);
  exit(0);
}

int main(int argc , char *argv[]) {

    if(argc != 3){
      printf("You need to provide a port number for server\n");
      return 0;
    }

    signal(SIGINT, sigintServer);
    int opt = 1, port;
    int master_socket , addrlen , new_socket , client_socket[30], max_clients = 30 , activity, i , valread , sd;
    int max_sd, position, nport, ccli;
    struct sockaddr_in address, cli;
    char msg[10];
    char ip[20];

    char buffer[1025];  //data buffer of 1K

    //set of socket descriptors
    fd_set readfds;

    if(strncmp(argv[1],"-p",2) == 0){
      port = atoi(argv[2]);
    }
    //initialise all client_socket[] to 0 so not checked
    for (i = 0; i < max_clients; i++){
        client_socket[i] = 0;
    }

    //create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) == 0){
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ){
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //get server ip
    char hostbuffer[256];
    char *IPbuffer;
    struct hostent *host_entry;
    int hostname;

    // To retrieve hostname
    hostname = gethostname(hostbuffer, sizeof(hostbuffer));
    checkHostName(hostname);

    // To retrieve host information
    host_entry = gethostbyname(hostbuffer);
    checkHostEntry(host_entry);

    // To convert an Internet network
    // address into ASCII string
    IPbuffer = inet_ntoa(*((struct in_addr*)
                           host_entry->h_addr_list[0]));

    //type of socket created
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = inet_addr(IPbuffer);
    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address)) < 0 ){
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d %s\n", ntohs(address.sin_port),inet_ntoa(address.sin_addr));

    //try to specify maximum of 3 pending connections for the master socket
    if (listen(master_socket, 3) < 0){
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //accept the incoming connection
    addrlen = sizeof(address);

    while(1){
        //clear the socket set
        FD_ZERO(&readfds);

        //add master socket to set
        FD_SET(master_socket, &readfds);
        max_sd = master_socket;

        //add child sockets to set
        for ( i = 0 ; i < max_clients ; i++){
            //socket descriptor
            sd = client_socket[i];

            //if valid socket descriptor then add to read list
            if(sd > 0){
                FD_SET( sd , &readfds);
            }
            //highest file descriptor number, need it for the select function
            if(sd > max_sd){
                max_sd = sd;
            }
        }

        //wait for an activity on one of the sockets , timeout is NULL ,
        //so wait indefinitely
        activity = select( max_sd + 1 , &readfds , NULL , NULL , NULL);

        if ((activity < 0) && (errno!=EINTR)){
            printf("select error");
        }

        //If something happened on the master socket ,
        //then its an incoming connection
        if (FD_ISSET(master_socket, &readfds)){
            if ((new_socket = accept(master_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0){
                perror("accept");
                exit(EXIT_FAILURE);
            }

            // //inform user of socket number - used in send and receive commands
            //add new socket to array of sockets
            for (i = 0; i < max_clients; i++){
                //if position is empty
                if( client_socket[i] == 0 ){
                    client_socket[i] = new_socket;
                    position = i;
                    break;
                }
            }
        }

        //else its some IO operation on some other socket
        for (i = 0; i < max_clients; i++){
            sd = client_socket[i];

            if (FD_ISSET( sd , &readfds)){
                //Check if it was for closing , and also read the
                //incoming message
                if ((valread = recv( sd , buffer, sizeof(buffer), 0)) == 0){
                    //Somebody disconnected , get his details and print
                    getpeername(sd , (struct sockaddr*)&address, (socklen_t*)&addrlen);

                    memset(ip,'\0',sizeof(ip));

                    strcpy(ip, inet_ntoa(address.sin_addr));
                    nport = ntohs(address.sin_port);
                    // sscanf(buffer, "%s %s %d", msg, ip, &nport);
                    char temp[1025];
                    if(!clientExists(head, ip, nport)){
                      printf("ERROR_IP_PORT_NOT_FOUND_IN_LIST\n");
                    }
                    else{
                      deleteNode(&head, ip, nport);
                      for(int k = 0; k < max_clients; k++){
                        if(ccli != client_socket[k] && client_socket[k] != 0){
                          sprintf(temp,"USER_OFF< %s %d>", ip, nport);
                          send(client_socket[k], temp, sizeof(temp), 0);
                        }
                      }
                    }
                    //Close the socket and mark as 0 in list for reuse
                    close(sd);
                    client_socket[i] = 0;
                }

                //Do thing with the incoming message
                else{

                    //make it a string
                    buffer[valread] = '\0';
                    //LOG ON MESSAGE RECEIVED
                    if((strncmp(buffer, "LOG_ON", 6) == 0)){
                      memset(msg,'\0',sizeof(msg));
                      memset(ip,'\0',sizeof(ip));
                      sscanf(buffer, "%s %s %d", msg, ip, &nport);
                      if(!clientExists(head, ip, nport)){
                        Insert(&head, ip, nport);
                      }
                      char temp[1025];

                      memset(temp,'\0',sizeof(temp));
                      //fix the new socket to send to the client
                      if((ccli = socket(AF_INET, SOCK_STREAM, 0)) == -1){
                        perror("socket");
                      }
                      if( setsockopt(ccli , SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ){
                          perror("setsockopt");
                      }
                      cli.sin_family = AF_INET;
                      cli.sin_port = htons(nport);
                      cli.sin_addr.s_addr = inet_addr(ip);

                      if ((connect(ccli, (struct sockaddr *)&cli, sizeof(cli))) != 0){
                        perror("connect");
                      }

                      client_socket[position] = ccli;

                      if(ccli > 0){
                        FD_SET( ccli , &readfds);
                      }
                      if(ccli > max_sd){
                          max_sd = ccli;
                      }

                      //make string to send
                      sprintf(temp,"USER_ON <%s,%d>", ip, nport);
                      //send to every other client
                      for(int k = 0; k < max_clients; k++){
                        if(ccli != client_socket[k] && client_socket[k] != 0){
                          send(client_socket[k], temp, sizeof(temp), 0);
                        }
                        else if(ccli == client_socket[k]){
                          //send to the client to unblock it
                          send(ccli,"unblock",strlen("unblock"),0);
                        }
                      }

                      //send client list after user on sent
                      memset(buffer,'\0',sizeof(buffer));
                      valread = recv( sd , buffer, sizeof(buffer), 0);

                      buffer[valread] = '\0';

                      if(strncmp(buffer, "GET_CLIENTS", 11) == 0){
                        char temp[1025];
                        char data[1025];
                        int num;      //number of clients
                        //get the list of client as a string
                        num = getClient(head, ip, nport, temp);

                        memset(data,'\0',sizeof(data));
                        sprintf(data,"CLIENT_LIST %d %s",num,temp);

                        //send data
                        send(ccli, data, sizeof(data), 0);

                      }
                    }
                    else if(strncmp(buffer, "LOG_OFF", 7) == 0){

                      memset(msg,'\0',sizeof(msg));
                      memset(ip,'\0',sizeof(ip));

                      sscanf(buffer, "%s %s %d", msg, ip, &nport);
                      char temp[1025];
                      if(!clientExists(head, ip, nport)){
                        printf("ERROR_IP_PORT_NOT_FOUND_IN_LIST\n");
                      }
                      else{
                        deleteNode(&head, ip, nport);
                        for(int k = 0; k < max_clients; k++){
                          if(ccli != client_socket[k] && client_socket[k] != 0){
                            sprintf(temp,"USER_OFF <%s,%d>", ip, nport);
                            send(client_socket[k], temp, sizeof(temp), 0);
                          }
                        }
                      }
                      // printList(head);
                    }

                    memset(buffer,'\0',sizeof(buffer));
                }
            }
        }
    }

    return 0;
}
