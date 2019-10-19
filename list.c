#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "./headers/list.h"

void Insert(struct Node** head, char* newIp, int newPort){
  struct Node *temp, *newNode;

  newNode = malloc(sizeof(struct Node));

  newNode->ipAddress = malloc(strlen(newIp) + 1);
  newNode->next = NULL;

  strcpy(newNode->ipAddress,newIp);
  newNode->portNum = newPort;
  //first client
  if(*head == NULL){
    *head = newNode;
  }
  else{

    temp = *head;

    //Find the last Node of list
    while(temp->next){
      temp = temp->next;
    }

    //Insert the new Node at the end of the list
    temp->next = newNode;
  }
}

int getClient(struct Node* head, char* ip, int port, char* string){
  struct Node* current = head;
  int num = 0;
  char temp[1025];
  string[0] = '\0';
  //loop through the list
  while(current != NULL){
    if((strcmp(current->ipAddress,ip) != 0) || (current->portNum != port)){
      //make the string to be sent to the client
      num++;
      strcat(string,"<");
      sprintf(temp,"%s,%d", current->ipAddress, current->portNum);
      strcat(string,temp);
      strcat(string,">");
    }
    current = current->next;
  }
  return num;
}

void printList(struct Node* head){
  if(head == NULL){
    printf("Empty List\n");
  }
  else {
    while(head){
      printf("client IP address: %s\n",head->ipAddress);
      printf("client portNum: %d\n",head->portNum);
      head = head->next;
    }
  }
}

void deleteList(struct Node** head){

   struct Node* current = *head;
   struct Node* next = NULL;

  //loop through the list
   while (current != NULL){
       next = current->next;
       free(current->ipAddress);
       free(current);
       current = next;
   }

   *head = NULL;
}


void deleteNode(struct Node** head, char* ip, int port){
      // Store head node
      struct Node* temp = *head, *prev;

      // If head node itself holds the key to be deleted
      if (temp != NULL && (temp->portNum == port && (strcmp(temp->ipAddress,ip) == 0))){
          *head = temp->next;   // Changed head
          free(temp->ipAddress);
          free(temp);               // free old head
          return;
      }

      // Search for the key to be deleted, keep track of the
      // previous node as we need to change 'prev->next'
      while (temp != NULL && ((temp->portNum != port) || ((strcmp(temp->ipAddress,ip) != 0)))){
          prev = temp;
          temp = temp->next;
      }

      // If key was not present in linked list
      if (temp == NULL) return;

      // Unlink the node from linked list
      prev->next = temp->next;

      free(temp->ipAddress);
      free(temp);  // Free memory
}

int clientExists(struct Node* head, char* newIp, int newPort){
  struct Node* current = head;

  //loop through the list
  while(current != NULL){
    if((strcmp(current->ipAddress,newIp) == 0) && (current->portNum == newPort)){
      return 1;
    }
    current = current->next;
  }
  return 0;
}
