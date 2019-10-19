all: dropbox_server dropbox_client

dropbox_server: server.o list.o clientFunctions.o
	gcc -g -Wall server.o list.o clientFunctions.o -o dropbox_server -l pthread

dropbox_client: client.o list.o clientFunctions.o
	gcc -g -Wall client.o list.o clientFunctions.o -o dropbox_client -l pthread

server.o: server.c
	gcc -c -g server.c

clientFunctions.o: clientFunctions.c
	gcc -c -g clientFunctions.c

client.o: client.c ./headers/client.h
	gcc -c -g client.c

list.o: list.c ./headers/list.h
	gcc -c -g list.c

clean:
	rm -rf *.o dropbox_server dropbox_client
