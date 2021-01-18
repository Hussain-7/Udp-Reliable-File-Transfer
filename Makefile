all: Sender Reciever

Sender:
	gcc -Wall -Werror -g -o Sender server.c

Reciever:
	gcc -Wall -Werror -g -o Reciever client.c