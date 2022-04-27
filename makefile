FLAGS = -std=gnu11 -g -Wall -Wextra
VALGRIND =  valgrind --track-origins=yes --track-fds=yes --malloc-fill=0x40 --free-fill=0x23 --leak-check=full --show-leak-kinds=all


all: server client

server: upush_server.c
	gcc $(FLAGS) upush_server.c -o upush_server
	$(VALGRIND) ./upush_server 12345 0

client: upush_client.c
	gcc $(FLAGS) upush_client.c -o upush_client
	$(VALGRIND) ./upush_client

clean:
	rm upush_client
	rm upush_server

