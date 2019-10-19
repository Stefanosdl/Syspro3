typedef struct serInfo{
    char dirname[30];
    int sPort;
    int cPort;
    int workers;
    int buffer;
    char ip[20];
}serInfo;

struct buffer{
  char pathname[128];
  char version[64];
  char ip[20];
  int portNum;
};

struct bufferArray{
  struct buffer* arguments;
  int start;
  int end;
  int count;
};

void *threadsHandler(void *);
void *mainThread(void *);
void initialize(int);
void place(struct buffer);
struct buffer obtain();
void getFileList(char* , int, char*, char*);
char* timestamp(char*, char*);
void getFile(char* , int , char* , char*, int);
long int findSize(char* );
void checkHostName(int );
void checkHostEntry(struct hostent* );
void checkIPbuffer(char* );
void InsertInBufferOne();
void _mkdir(const char* );
void sendfile(FILE* , int);
void writefile(FILE* , int);
char *strremove(char *, const char *);
int timeComparison(char* , char*);

#define Max_Path 3000
#define Transfer_Size 1024
