all: Sender Reciever

Sender:
	gcc -Wall -Werror -g -o Sender Sender.c

Reciever:
	gcc -Wall -Werror -g -o Reciever Reciever.c