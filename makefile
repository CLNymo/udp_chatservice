FLAGS = -std=gnu11 -g -Wall -Wextra
VALGRIND =  valgrind --track-origins=yes --track-fds=yes --malloc-fill=0x40 --free-fill=0x23 --leak-check=full --show-leak-kinds=all


all: server client send_packet

server: upush_server.c send_packet.c
	gcc $(FLAGS) upush_server.c send_packet.c -o upush_server
	$(VALGRIND) ./upush_server 12345 0

client1: upush_client.c send_packet.c
	gcc $(FLAGS) upush_client.c send_packet.c -o upush_client
	$(VALGRIND) ./upush_client A 127.0.0.1 12345 5 20

client2: upush_client.c send_packet.c
	gcc $(FLAGS) upush_client.c send_packet.c -o upush_client
	$(VALGRIND) ./upush_client B 127.0.0.1 12345 5 20

client3: upush_client.c send_packet.c
	gcc $(FLAGS) upush_client.c send_packet.c -o upush_client
	$(VALGRIND) ./upush_client C 127.0.0.1 12345 5 20



clean:
	rm upush_client
	rm upush_server

