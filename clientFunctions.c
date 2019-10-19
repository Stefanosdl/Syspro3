#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include "./headers/client.h"
#include "./headers/list.h"

int timeComparison(char* first, char* second){
  //f pointer is for the new version to be checked
  char *fendptr, *sendptr;
  int base = 10;
  int fhours, fminutes, fseconds;
  int shours, sminutes, sseconds;
  char buf[2];

  fhours = strtol(first, &fendptr, base);
  shours = strtol(second, &sendptr, base);
  fminutes = strtol(fendptr + 1, &fendptr, base);
  sminutes = strtol(sendptr + 1, &sendptr, base);
  fseconds = strtol(fendptr + 1, &fendptr, base);
  sseconds = strtol(sendptr + 1, &sendptr, base);

  if(fhours > shours){
    return 1;
  }
  else if (fhours == shours){
    if(fminutes > sminutes){
      return 1;
    }
    else if(fminutes == sminutes){
      if(fseconds > sseconds){
        return 1;
      }
    }
  }
  return 0;
}

// Returns hostname for the local computer
void checkHostName(int hostname){
    if (hostname == -1){
        perror("gethostname");
        exit(1);
    }
}

// Returns host information corresponding to host name
void checkHostEntry(struct hostent * hostentry){
    if (hostentry == NULL){
        perror("gethostbyname");
        exit(1);
    }
}

// Converts space-delimited IPv4 addresses
// to dotted-decimal format
void checkIPbuffer(char *IPbuffer){
    if (NULL == IPbuffer){
        perror("inet_ntoa");
        exit(1);
    }
}


char* timestamp(char *filePath, char* version){
  struct stat attrib;
  stat(filePath, &attrib);
  //check when last modified
  // strftime(version, 100, "%x-%H:%M%p", localtime(&(attrib.st_ctime)));
  strftime(version, 100, "%d.%m.%Y-%H:%M:%S", localtime(&attrib.st_mtime));
  // printf("----%s %s\n",filePath,version);
  return version;
}


void getFileList(char* dirname, int indent, char* counter, char* fileList){
  DIR *dir;
  struct dirent *entry;
  char path[1025];
  char version[100];
  char p[1025];
  int c;
  struct stat info;
  if ((dir = opendir(dirname)) == NULL){
    perror("opendir() error");
  }
  else {
    while ((entry = readdir(dir)) != NULL) {
      if (entry->d_name[0] != '.') {
        strcpy(path, dirname);
        strcat(path, "/");
        strcat(path, entry->d_name);
        if (stat(path, &info) != 0){
          fprintf(stderr, "stat() error on %s: %s\n", path, strerror(errno));
        }
        else if(S_ISDIR(info.st_mode)){
          getFileList(path, indent+1, counter, fileList);
        }
        else if(S_ISREG(info.st_mode)){
          //we found a file
          c = atoi(counter);
          c++;
          sprintf(counter,"%d",c);
          sprintf(p,"<%s,%s> ",path, timestamp(path,version));
          memset(version,'\0',sizeof(version));
          strcat(fileList,p);
        }
      }
    }
    closedir(dir);
  }
}

void getFile(char* dirname, int indent, char* file, char* newVersion, int sd){
  DIR *dir;
  struct dirent *entry;
  char path[1025];
  char version[100];
  char fileName[1025];
  char p[1025];
  char* ver;
  struct stat info;
  long int fileSize;

  memset(fileName,'\0',sizeof(fileName));
  if ((dir = opendir(dirname)) == NULL){
    perror("opendir() error");
  }
  else {
    while ((entry = readdir(dir)) != NULL) {
      if (entry->d_name[0] != '.') {
        strcpy(path, dirname);
        strcat(path, "/");
        strcat(path, entry->d_name);
        if (stat(path, &info) != 0){
          fprintf(stderr, "stat() error on %s: %s\n", path, strerror(errno));
        }
        else if(S_ISDIR(info.st_mode)){
          getFile(path, indent+1, file, newVersion, sd);
        }
        else if(S_ISREG(info.st_mode)){
          //we found a file
          //check if file exists
          if(strcmp(path,file) == 0){
            //check for version here
            memset(version,'\0',sizeof(version));
            ver = strchr(newVersion,'-');
            ver++;
            char* t = timestamp(file,version);

            t = strchr(t, '-');
            t++;

            if(timeComparison(ver,t) == 1){
              //if up to date
              memset(version,'\0',sizeof(version));
              send(sd, "FILE_UP_TO_DATE", strlen("FILE_UP_TO_DATE"), 0);
            }
            else{
              //get the size of the file
              fileSize = findSize(file);

              //make string to send

              FILE* fp = fopen(path,"r");
              char byte_buffer[Transfer_Size];
              fread(byte_buffer, 1, Transfer_Size-1, fp);
              sprintf(fileName,"FILE_SIZE %s %ld %s",timestamp(file,version),fileSize, byte_buffer);

              send(sd, fileName, sizeof(fileName), 0);

                // sendfile(fp, sd);
              fclose(fp);

              memset(fileName,'\0',sizeof(fileName));
              memset(byte_buffer,'\0',sizeof(byte_buffer));
            }
          }
        }
      }
    }
    closedir(dir);
  }
}

void sendfile(FILE* fp, int sd){
  int n,len;
  char byte_buffer[Transfer_Size];

  //read from file and send
  while((n = fread(byte_buffer, sizeof(char), Transfer_Size-1, fp)) > 0){
    byte_buffer[(sizeof(byte_buffer)) - 1] = 0;
    if(send(sd, byte_buffer, n, 0) < 0){
      break;
    }
    memset(byte_buffer, 0, Transfer_Size);
  }
}

void writefile(FILE* fp, int sd){
  ssize_t n;
  char byte_buffer[Transfer_Size];
  char *string = malloc(Transfer_Size+1);
  //get data and store it to file
  while(( n = recv(sd, string, Transfer_Size, 0)) > 0){
    strcpy(string,"");
    string++;
    if(fwrite(string, sizeof(char), n-1, fp) < 0){
      perror("fwrite");
    }
    memset(byte_buffer, 0, Transfer_Size);
  }
}


void _mkdir(const char *dir) {
        char tmp[Max_Path];
        char *p = NULL;
        size_t len;

        snprintf(tmp, sizeof(tmp),"%s",dir);
        len = strlen(tmp);
        if(tmp[len - 1] == '/'){
          tmp[len - 1] = 0;
        }
        for(p = tmp + 1; *p; p++)
          if(*p == '/') {
            *p = 0;
            mkdir(tmp, S_IRWXU);
            *p = '/';
          }
        mkdir(tmp, S_IRWXU);
}

char *strremove(char *str, const char *sub) {
    char *p, *q, *r;
    if ((q = r = strstr(str, sub)) != NULL) {
        size_t len = strlen(sub);
        while ((r = strstr(p = r + len, sub)) != NULL) {
            while (p < r)
                *q++ = *p++;
        }
        while ((*q++ = *p++) != '\0')
            continue;
    }
    return str;
}

long int findSize(char* fileName){
  // opening the file in read mode
  FILE* fp = fopen(fileName, "r");

  // checking if the file exist or not
  if (fp == NULL){
    perror("error");
  }

  fseek(fp, 0L, SEEK_END);

  // calculating the size of the file
  long int res = ftell(fp);

  // closing the file
  rewind(fp);
  fclose(fp);

  return res;
}
