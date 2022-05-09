#include "send_packet.h"
#include "client_list.c"
#include <ctype.h>

#define BUFMAX 48 // makslengde på melding til og fra klient vil aldri overstige dette
#define SEQMAX 8 // maks antall bytes vi tillater på sekvensnummer
#define MESSAGE_TYPE_MAX 6 // makslengde på meldingstype som er lengden til LOOKUP



struct sockaddr_in clientaddr, serveraddr;

// STRUCTS


// METODER
int check_char(char c);
void check_error(int res, char *msg);
void read_message(char *buf);
void register_client(char *nick);



// GLOBALE VARIABLER
int PORT;
struct client *client_list;
int sockfd;


int main(int argc, char *argv[]) {


  // TODO: feilmelding dersom melding er for lang?
  if (argc < 2){
    printf("usage: %s <port> <tapssannsynlighet>\n", argv[0]);
    return EXIT_SUCCESS;
  }

  PORT = atoi(argv[1]);

  srand48(time(NULL));
  set_loss_probability(atof(argv[2])/100);
  char buffer[BUFMAX] = {0};


  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  check_error(sockfd, "Failed to set up socket");


  memset(&clientaddr, 0, sizeof(clientaddr));
  memset(&serveraddr, 0, sizeof(serveraddr));

  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(PORT);
  serveraddr.sin_addr.s_addr = INADDR_ANY;

  int bindval = bind(sockfd, (const struct sockaddr*)&serveraddr, sizeof(serveraddr));
  check_error(bindval, "Failed to bind");


  socklen_t clientaddr_len = sizeof(struct sockaddr_in);

  while(1){ // serveren venter evig, som definert i oppgaven
    int rc = recvfrom(
      sockfd,
      buffer,
      BUFMAX,
      0,
      (struct sockaddr*)&clientaddr,
      &clientaddr_len
    );


    buffer[rc] = '\0';
    printf("Message received: %s\n", buffer); // Se om vi fikk meldingen.
    read_message(buffer);
  }

  close(sockfd);
  free_clients(client_list);
  return EXIT_SUCCESS;
}

/*
PARSE MELDING FRA CLIENT
*/

void read_message(char *msg){

  int sequence_number;
  char message_type[MESSAGE_TYPE_MAX + 1];
  char nick[NICKMAX + 1];
  char ack[BUFMAX];


  // TODO: sjekke at hvert inputfelt ikke overstiger avsatt buffer. Trenger vi tallene for width?
  int rc = sscanf(msg, "PKT %d %s %s", &sequence_number, message_type, nick);

  // Ugyldig melding mottatt:
  if(rc != 3){
    printf("Ignored a strange message: %s\n", msg);
    return;
  }

  message_type[strlen(message_type)] ='\0';
  nick[strlen(nick)] = '\0';

  printf("SEQ: %d\n", sequence_number);
  printf("type: %s\n", message_type);
  printf("nick: %s\n", nick);


  // REGISTRERINGS-MELDING
  if(strcmp(message_type, "REG") == 0){
    register_client(nick);

    // Formattere ACK
    sprintf(ack, "ACK %d OK", sequence_number);

  }

  // LOOKUP-MELDING
  else if (strcmp(message_type, "LOOKUP") == 0){
    struct client *client = lookup_client(client_list, nick);

    // oppslag godkjent
    if(client != NULL){
      char ip_string[16];
      rc = inet_ntop(AF_INET, &client->sockaddr->sin_addr, ip_string, sizeof(ip_string));
      check_error(rc, "inet_pton failed");

      // Formattere ack
      sprintf(ack, "ACK %d NICK %s IP %s PORT %d", sequence_number, client->nick, ip_string, client->sockaddr->sin_port);
      printf("nick found, ack is: %s\n", ack);
    }

    // Oppslag feilet
    else {
      // Formattere ack
      sprintf(ack, "ACK %d NOT FOUND", sequence_number);
      printf("nick not found, ack is: %s\n", ack );
    }
  }

  // send ACK
  send_packet(sockfd, ack, strlen(ack), 0, (struct sockaddr*)&clientaddr, sizeof(struct sockaddr_in));
  printf("sent response to client: %s\n", ack);
}

// legger til ny client, oppdaterer info hvis client finnes allerede
void register_client(char *nick){

  struct client *client = lookup_client(client_list, nick);
  if (client == NULL){
    client_list = add_client(client_list, nick, clientaddr.sin_port, clientaddr.sin_addr.s_addr);
    printf("%s port is: %d\n", nick, clientaddr.sin_port);
    print_linked_list(client_list);
  }
  else {
    // oppdater socket
    client->sockaddr->sin_port = clientaddr.sin_port;
    client->sockaddr->sin_addr.s_addr = clientaddr.sin_addr.s_addr;
    printf("client socket updated\n");
  }
}



/*
HJELPEMETODER
*/

int check_char(char c){
  if (!isalpha(c) && !isalnum(c) && !isspace(c)) {
    printf("illegal character is: %c\n", c);
    check_error(-1, "illegal character received");
    return -1;
  }
  else {
    return 0;
  }
}

void check_error(int res, char *msg){ // greit å ha siden mange metoder returnerer -1 ved error
  if (res == -1){
    perror(msg);
    exit(EXIT_FAILURE);
  }
}





