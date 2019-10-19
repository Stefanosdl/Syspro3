struct Node{
    char* ipAddress;
    int portNum;
    struct Node* next;
};

void Insert(struct Node** , char*, int);
int getClient(struct Node* ,char* , int, char*);
int clientExists(struct Node* , char*, int);
void printList(struct Node* );
void deleteNode(struct Node **, char*, int);
void deleteList(struct Node**);
