default: server.out client.out

client.out: client.c
	gcc client.c -g -o client.out -lm
server.out: server.c
	gcc server.c -g -o server.out -lpthread -lm