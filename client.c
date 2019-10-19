#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "./headers/client.h"
#include "./headers/list.h"

int sockCli, sockSer, new_socket, bufsize, mySock;
struct Node* head = NULL;
pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;
pthread_cond_t cond;
pthread_mutex_t mutex;
pthread_mutex_t mtx;
struct bufferArray threadsBuffer;

void sigintClient(int sig){
  send(sockSer, "LOG_OFF", strlen("LOG_OFF"), 0);
  free(threadsBuffer.arguments);
  deleteList(&head);
  close(sockCli);
  close(sockSer);
  exit(0);
}

void initialize(int buffersize){
  bufsize = buffersize;
  threadsBuffer.arguments = malloc(buffersize*(sizeof(struct buffer)));
  threadsBuffer.start = 0;
  threadsBuffer.end = -1;
  threadsBuffer.count = 0;
}

void place(struct buffer data){
  pthread_mutex_lock(&mtx);
  while ( threadsBuffer.count >= bufsize ){
    pthread_cond_wait(&cond_nonfull , &mtx);
  }
  threadsBuffer.end = (threadsBuffer.end + 1) % bufsize;
  threadsBuffer.arguments[threadsBuffer.end] = data;
  threadsBuffer.count++;
  pthread_mutex_unlock(&mtx);
  pthread_cond_signal(&cond_nonempty);
}

struct buffer obtain(){
  struct buffer data;
  pthread_mutex_lock(&mtx);
  while(threadsBuffer.count <= 0){
    pthread_cond_wait (&cond_nonempty, &mtx);
  }
  data = threadsBuffer.arguments[threadsBuffer.start];
  threadsBuffer.start = (threadsBuffer.start + 1) % bufsize;
  threadsBuffer.count--;
  pthread_mutex_unlock(&mtx);
  pthread_cond_signal(&cond_nonfull);
  return data;
}


void InsertInBufferOne(char* ip, int portNum){
  struct buffer data;

  strcpy(data.pathname,"-1");
  strcpy(data.version,"-1");
  strcpy(data.ip, ip);
  data.portNum = portNum;
  place(data);
}



void *threadsHandler(void *arg){
  serInfo *siPtr;
  siPtr = (serInfo*)arg;
  char buffer[1024];
  memset(buffer,'\0',sizeof(buffer));

  while(1){
      struct buffer data = obtain();
      //if pathname is NULL
      if(strcmp(data.pathname,"-1") == 0){
        //then its not file
        //send GET_FILE_LIST
        int newCli, valread, opt = 1;
        struct sockaddr_in client_address;
        if((newCli = socket(AF_INET, SOCK_STREAM, 0)) == -1){
          perror("socket");
        }
        else{
          if( setsockopt(newCli, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ){
              perror("setsockopt");
          }
          else{
              client_address.sin_family = AF_INET;
              client_address.sin_port = htons(data.portNum);    //client port
              client_address.sin_addr.s_addr = inet_addr(data.ip);


              if ((connect(newCli, (struct sockaddr *)&client_address, sizeof(client_address))) == 0){
                send(newCli, "GET_FILE_LIST", strlen("GET_FILE_LIST"), 0);
                valread = recv(newCli, buffer, sizeof(buffer), 0);
                //make it a string
                buffer[valread] = '\0';
                char fileList2[1025];
                char* counter;
                char* pathname;
                char* version;
                char* string;
                char* string2;

                memset(fileList2,'\0',sizeof(fileList2));
                counter = strchr(buffer, ' ');
                int counter2 = atoi(counter);

                strcpy(fileList2,buffer);
                string = strtok(buffer, ">");
                string2 = strtok(fileList2, ",");
                //tokenize
                while((string != NULL) && ((string2 != NULL)  && counter2 > 0)){
                  if(counter2 == atoi(counter)){
                    version = strchr(string, ',');
                    version++;
                  }
                  else{
                    strcpy(version,string);
                  }

                  //get the ids
                  if(counter2 != 0){
                    pathname = strchr(string2,'<');
                    pathname++;
                    struct buffer newData;

                    //make new data
                    strcpy(newData.pathname,pathname);
                    strcpy(newData.version,version);
                    strcpy(newData.ip,data.ip);
                    newData.portNum = data.portNum;

                    //insert into buffer
                    place(newData);
                    pthread_cond_signal(&cond_nonempty);
                    string2 = strtok(NULL, ",");
                  }
                  string = strtok(NULL, ">");
                  counter2--;
                }
                memset(buffer,'\0',sizeof(buffer));
                close(newCli);
              }
          }
        }
      }
      else{
        //check if user exists
        pthread_mutex_lock(&mutex);
        if(!clientExists(head, data.ip, data.portNum)){
          perror("Client does not exists");
          pthread_exit(NULL);
        }
        //user exists go on
        pthread_mutex_unlock(&mutex);
        char folder[200];
        memset(folder, '\0', sizeof(folder));
        //make path
        sprintf(folder,"%s_%d/%s_%d/%s",siPtr->ip,siPtr->cPort,data.ip,data.portNum,data.pathname);

        //check if file exists locally
        struct stat sb;
        if(stat(folder,&sb) == 0){
          //exists
          int newCli, valread, opt = 1;
          struct sockaddr_in client_address;
          if((newCli = socket(AF_INET, SOCK_STREAM, 0)) == -1){
            perror("socket");
          }
          else{
            if( setsockopt(newCli, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ){
                perror("setsockopt");
            }
            else{
              client_address.sin_family = AF_INET;
              client_address.sin_port = htons(data.portNum);    //client port
              client_address.sin_addr.s_addr = inet_addr(data.ip);
              if ((connect(newCli, (struct sockaddr *)&client_address, sizeof(client_address))) == 0){
                char sendb[1025];
                char newV[1025];
                memset(sendb,'\0',sizeof(sendb));
                memset(newV,'\0',sizeof(newV));
                sprintf(newV,"%s",timestamp(folder,newV));
                sprintf(sendb,"GET_FILE< %s %s >", data.pathname, newV);
                send(newCli, sendb, sizeof(sendb), 0);
                valread = recv(newCli, buffer, sizeof(buffer), 0);

                //check if up to date
                if(strcmp(buffer,"FILE_UP_TO_DATE") == 0){
                  //file is up to date.don't do anything
                }
                else{
                  char temp[100];
                  char msg[15];
                  char version[50];
                  char bytes[Transfer_Size];
                  int size;
                  char* s;

                  sscanf(buffer,"%s %s %d",msg, version, &size);
                  sprintf(bytes,"%s %s %d ",msg,version,size);
                  s = strremove(buffer,bytes);

                  FILE *ft;
                  ft = fopen(folder, "w");
                  fwrite(s,1,strlen(s),ft);
                  fclose(ft);

                  memset(msg,'\0',sizeof(msg));
                  memset(bytes,'\0',sizeof(bytes));
                  memset(version,'\0',sizeof(version));
                  memset(newV,'\0',sizeof(newV));
                  memset(temp,'\0',sizeof(temp));
                }

                memset(buffer, '\0', sizeof(buffer));
                close(newCli);
              }
            }
          }
        }
        else{
          //it does not exist so we send GET_FILE message to the client

          int newCli, valread, opt = 1;
          struct sockaddr_in client_address;
          if((newCli = socket(AF_INET, SOCK_STREAM, 0)) == -1){
            perror("socket");
          }
          else{
            if( setsockopt(newCli, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ){
                perror("setsockopt");
            }
            else{
                client_address.sin_family = AF_INET;
                client_address.sin_port = htons(data.portNum);    //client port
                client_address.sin_addr.s_addr = inet_addr(data.ip);

                if ((connect(newCli, (struct sockaddr *)&client_address, sizeof(client_address))) == 0){
                  char sendb[1025];
                  memset(sendb,'\0',sizeof(sendb));
                  sprintf(sendb,"GET_FILE< %s %d >", data.pathname, -1);

                  send(newCli, sendb, sizeof(sendb), 0);
                  valread = recv(newCli, buffer, sizeof(buffer), 0);

                  char* file;
                  char* dirPath;
                  char* s;
                  char dirname[100];
                  char newPath[100];
                  char newFile[100];
                  char temp[100];
                  char msg[15];
                  char version[50];
                  char bytes[Transfer_Size];
                  int size;
                  memset(temp,'\0',sizeof(temp));
                  struct stat path_stat;
                  //get the file size from socket message
                  sscanf(buffer,"%s %s %d",msg, version, &size);
                  sprintf(bytes,"%s %s %d ",msg,version,size);
                  s = strremove(buffer,bytes);

                  file = strrchr(data.pathname,'/');
                  file++;

                  strcpy(dirname,data.pathname);
                  dirPath = strtok(dirname,"/");
                  int count = 1;
                  //tokenize to get directory path and file
                  while( dirPath != NULL ) {
                    if(count == 1){
                      strcat(temp,dirPath);
                      stat(temp, &path_stat);
                      if(S_ISDIR(path_stat.st_mode) == 1){
                        strcpy(newPath,temp);
                      }
                      else if(S_ISREG(path_stat.st_mode) == 1){
                        strcpy(newFile,dirPath);
                      }
                    }
                    else{
                      strcat(temp,"/");
                      strcat(temp,dirPath);
                      stat(temp, &path_stat);
                      if(S_ISDIR(path_stat.st_mode) == 1){
                        strcpy(newPath,temp);
                      }
                      else if(S_ISREG(path_stat.st_mode) == 1){
                        strcpy(newFile,dirPath);
                      }

                    }
                    count = 0;
                    dirPath = strtok(NULL, "/");
                  }

                  //check if it is a sub dir

                  char folder[200];
                  memset(folder, '\0', sizeof(folder));
                  //make path
                  sprintf(folder,"%s_%d/%s_%d/%s",siPtr->ip,siPtr->cPort,data.ip,data.portNum,newPath);
                  //make directory if not exist
                  _mkdir(folder);

                  FILE* fp;
                  //make the path to file so we can create it
                  strcat(folder,"/");
                  strcat(folder,newFile);
                  fp = fopen(folder,"w");

                  fwrite(s,1,strlen(s),fp);

                  fclose(fp);


                  close(newCli);

                  memset(buffer,'\0',sizeof(buffer));
                  memset(newPath,'\0',sizeof(newPath));
                  memset(newFile,'\0',sizeof(newFile));
                  memset(msg,'\0',sizeof(msg));
                  memset(bytes,'\0',sizeof(bytes));
                  memset(version,'\0',sizeof(version));
                  memset(temp,'\0',sizeof(temp));
                  memset(folder,'\0',sizeof(folder));

                }
              }
            }
          }
        }
      }

  pthread_exit(NULL);
}



void *mainThread(void *arg){
  struct sockaddr_in server_address, client_address;
  serInfo *siPtr;
  int valread, opt = 1, client_socket[30], max_clients = 30, activity, sd, max_sd, i, position;

  //set of socket descriptors
  fd_set readfds;

  siPtr = (serInfo*)arg;

  //initialise all client_socket[] to 0 so not checked
  for (i = 0; i < max_clients; i++){
      client_socket[i] = 0;
  }
  if((sockCli = socket(AF_INET, SOCK_STREAM, 0)) == -1){
    perror("socket");
  }
  else{

      if( setsockopt(sockCli, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ){
          perror("setsockopt");
      }
      else{


      if((sockSer = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("socket");
      }
      if( setsockopt(sockSer, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0 ){
          perror("setsockopt");
      }

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


      server_address.sin_family = AF_INET;
      server_address.sin_port = htons(siPtr->sPort);    //server port

      server_address.sin_addr.s_addr = inet_addr(siPtr->ip);


      client_address.sin_family = AF_INET;
      client_address.sin_port = htons(siPtr->cPort);    //client port

      client_address.sin_addr.s_addr = inet_addr(IPbuffer);

      if ((connect(sockSer, (struct sockaddr *)&server_address, sizeof(server_address))) == 0){
        char msg[1025];
        char buffer[1025];
        int addrlen;

        memset(msg,'\0',sizeof(msg));
        sprintf(msg,"LOG_ON< %s %d>",inet_ntoa(client_address.sin_addr),ntohs(client_address.sin_port));

        send(sockSer, msg, strlen(msg), 0);
        //bind the socket to localhost port 8888
        if (bind(sockCli, (struct sockaddr *)&client_address, sizeof(client_address)) < 0 ){
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        //try to specify maximum of 3 pending connections for the master socket
        if (listen(sockCli, 3) < 0){
            perror("listen");
            exit(EXIT_FAILURE);
        }
        addrlen = sizeof(client_address);
        //select

        while(1){

          //clear the socket set
          FD_ZERO(&readfds);

          //add master socket to set
          FD_SET(sockCli, &readfds);
          max_sd = sockCli;

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
          if (FD_ISSET(sockCli, &readfds)){
              if ((new_socket = accept(sockCli, (struct sockaddr *)&client_address, (socklen_t*)&addrlen))<0){
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

          for (i = 0; i < max_clients; i++){
              sd = client_socket[i];

              if (FD_ISSET( sd , &readfds)){
                  //Check if it was for closing , and also read the
                  //incoming message
                  if ((valread = recv( sd , buffer, sizeof(buffer), 0)) == 0){
                      //Somebody disconnected
                      //Close the socket and mark as 0 in list for reuse
                      // close( sd );
                      client_socket[i] = 0;
                  }

                  //Echo back the message that came in
                  else{
                    //set the string terminating NULL byte on the end
                    //of the data read
                    buffer[valread] = '\0';

                    if(strncmp(buffer,"USER_ON",7) == 0){
                      char* newPort;
                      char* newIp;

                      //get ip
                      newIp = strrchr(buffer,'<');
                      newIp = strtok(newIp,",");
                      newIp++;

                      //get port
                      newPort = strrchr(buffer,',');
                      newPort = strtok(newPort,">");

                      pthread_mutex_lock(&mutex);
                      Insert(&head, newIp, atoi(newPort));
                      pthread_mutex_unlock(&mutex);

                      InsertInBufferOne(newIp, atoi(newPort));
                      // printList(head);

                    }
                    //else its the unblock message from server
                    //and we need to get the clients
                    else if(strncmp(buffer,"unblock",7) == 0){
                      send(sockSer, "GET_CLIENTS", strlen("GET_CLIENTS"), 0);
                    }
                    else if(strncmp(buffer,"CLIENT_LIST",11) == 0){
                      char* num;
                      char tbuffer[1025];
                      strcpy(tbuffer,buffer);

                      //get num of clients
                      num = strchr(buffer,' ');
                      num++;
                      num = strtok(num, " ");
                      //check if this is the only client
                      if(strcmp(num,"0") == 0){
                        //do nothing because its the only client on the system
                      }
                      else{
                        //get ip and port numbers
                        char* newPort;
                        char* newIp;
                        char* string;
                        char* string2;
                        char data[1025];
                        char data2[1025];
                        int counter = atoi(num);
                        string = strchr(tbuffer,'<');
                        string2 = strchr(tbuffer,'<');
                        strcpy(data,string);
                        strcpy(data2,string2);

                        //get the ports
                        string = strtok(data, ">");
                        string2 = strtok(data2,",");
                        while((string != NULL) && ((string2 != NULL)  && counter > 0)){
                          if(counter == atoi(num)){
                            newPort = strchr(string, ',');
                            newPort++;
                          }
                          else{
                            strcpy(newPort,string);
                          }

                          //get the ids
                          if(counter != 0){
                            newIp = strchr(string2,'<');
                            newIp++;

                            //we have both ip and port of the clients
                            //insert into the list
                            pthread_mutex_lock(&mutex);
                            Insert(&head, newIp, atoi(newPort));
                            pthread_mutex_unlock(&mutex);

                            InsertInBufferOne(newIp, atoi(newPort));
                            pthread_cond_signal(&cond_nonempty);
                            // printList(head);
                            string2 = strtok(NULL, ",");
                          }
                          string = strtok(NULL, ">");
                          counter--;
                        }
                      }
                    }
                    else if(strncmp(buffer, "USER_OFF", 8) == 0){
                      char msg[1025];
                      char ip[1025];
                      int nport;

                      memset(msg,'\0',sizeof(msg));
                      memset(ip,'\0',sizeof(ip));
                      sscanf(buffer, "%s %s %d", msg, ip, &nport);

                      pthread_mutex_lock(&mutex);
                      deleteNode(&head, ip, nport);
                      pthread_mutex_unlock(&mutex);

                    }
                    else if(strncmp(buffer, "GET_FILE_LIST", 13) == 0){
                      char tuple[1025];
                      char fileList[1025];
                      char fileList2[1025];

                      memset(fileList,'\0',sizeof(fileList));
                      memset(fileList2,'\0',sizeof(fileList2));
                      memset(tuple,'\0',sizeof(tuple));
                      char counter[3];
                      memset(counter,'\0',sizeof(counter));
                      sprintf(counter,"%d",0);
                      getFileList(siPtr->dirname,0,counter,tuple);

                      sprintf(fileList,"FILE_LIST %s %s",counter,tuple);
                      send(sd , fileList, sizeof(fileList), 0);

                    }
                    else if(strncmp(buffer, "GET_FILE", 8) == 0){
                      char pathname[1025];
                      char msg[200];
                      char version[3];
                      memset(pathname,'\0',sizeof(pathname));
                      memset(msg,'\0',sizeof(msg));
                      memset(version,'\0',sizeof(version));


                      sscanf(buffer, "%s %s %s", msg, pathname, version);
                      //we now have pathname and version
                      // and we call getfile function to do the rest of the job
                      getFile(siPtr->dirname, 0, pathname, version, sd);
                    }


                    memset(buffer,'\0',sizeof(buffer));
                  }
              }
          }
        }
      }
      else{
        perror("Cannot connect to server");
      }
    }
  }
  pthread_exit(NULL);

  close(sockCli);
}


int main(int argc, char *argv[]) {

  // signal(SIGINT, sigintClient);
  int workers, buffer, serverPort;
  pthread_t *threads;
  serInfo si;

  if(argc != 13){
    printf("Some arguments are missing\n");
    return 0;
  }

  //Handle the arguments from the command line
  for(int arg = 1; arg < argc; arg++){
    //if we are not at the last arg
    if(arg < argc - 1){
      if(strncmp(argv[arg], "-d", 2) == 0){
        strcpy(si.dirname, argv[arg + 1]);
      }
      else if(strncmp(argv[arg], "-p", -2) == 0){
        si.cPort = atoi(argv[arg + 1]);
      }
      else if(strncmp(argv[arg], "-w", -2) == 0){
        si.workers = atoi(argv[arg + 1]);
      }
      else if(strncmp(argv[arg], "-b", -2) == 0){
        si.buffer = atoi(argv[arg + 1]);
      }
      else if(strncmp(argv[arg], "-sp", -3) == 0){
        si.sPort = atoi(argv[arg + 1]);
      }
      else if(strncmp(argv[arg], "-sip", -4) == 0){
        strcpy(si.ip, argv[arg + 1]);
      }
    }
  }


  initialize(si.buffer);

  pthread_cond_init(&cond, NULL);
  pthread_cond_init(&cond_nonfull, NULL);
  pthread_cond_init(&cond_nonempty, NULL);
  pthread_mutex_init(&mutex, NULL);
  pthread_mutex_init(&mtx, NULL);
  //create main thread

  threads = malloc((si.workers+1) * sizeof(pthread_t));

  pthread_create(&threads[0], NULL, mainThread, &si);

  //create the rest of the threads
  for(int i=1; i < si.workers+1; i++){
    pthread_create(&threads[i], NULL, threadsHandler, &si);
  }

    //wait till threads finish their workers
  for(int i=0; i < si.workers+1; i++){
    pthread_join(threads[i], NULL);
  }


  pthread_cond_destroy(&cond);
  pthread_cond_destroy(&cond_nonfull);
  pthread_cond_destroy(&cond_nonempty);
  pthread_mutex_destroy(&mutex);
  pthread_mutex_destroy(&mtx);
  free(threads);
  return 0;
}
