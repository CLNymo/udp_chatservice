#include "send_packet.h"
#include <ctype.h>

#define BUFMAX 48 // makslengde på melding fra klient vil aldri overstige dette
#define SEQMAX 8 // maks antall bytes vi tillater på sekvensnummer
#define NICKMAX 20 //makslengde vi tillater på nick
#define MESSAGE_TYPE_MAX 6 // makslengde på meldingstype som er lengden til LOOKUP

int PORT;

struct sockaddr_in clientaddr, serveraddr;

// STRUCTS
typedef struct client {
  char *nick;
  struct in_addr* ip;
  int port;
  struct client *next;
};

// METODER
int check_char(char c);
void check_error(int res, char *msg);
void read_message(char *buf);
void register_client(char *nick);
void add_client(char *nick);
void print_linked_list(struct client *client);
struct client * lookup_nick(char *nick);
struct client * lookup_client(struct client *client, char *nick);
void free_client(struct client *client);



// GLOBALE VARIABLER
struct client *client_list;
int sockfd;


int main(int argc, char *argv[]) {
  fd_set fds; // liste over fd'ene til select
  PORT = atoi(argv[1]);
  set_loss_probability(atoi(argv[2]));


  // TODO: feilmelding dersom melding er for lang?

  char buffer[BUFMAX] = {0};
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  if (sockfd == 0) {
    fprintf(stderr, "Failed to set up socket");
    exit(EXIT_FAILURE);
  }
  memset(&clientaddr, 0, sizeof(clientaddr));
  memset(&serveraddr, 0, sizeof(serveraddr));

  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(PORT);
  serveraddr.sin_addr.s_addr = INADDR_ANY;

  int bindval = bind(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr));

  if (bindval == -1) {
    fprintf(stderr, "Failed to bind\n");
    exit(EXIT_FAILURE);
  }

  socklen_t clientaddr_len = sizeof(struct sockaddr_in);

  while(1){

    recvfrom(
      sockfd,
      buffer,
      BUFMAX,
      0,
      (struct sockaddr*)&clientaddr,
      &clientaddr_len); //Mottar melding fra client, lagrer addressen i clientaddr

      printf("Message received: %s\n", buffer); // Se om vi fikk meldingen.
      read_message(buffer);
  }

  close(sockfd);
  free_client(client_list);
  return EXIT_SUCCESS;
}

/*
PARSE MELDING FRA CLIENT
*/

void read_message(char *buffer){

  int sequence_number;
  char message_type[MESSAGE_TYPE_MAX + 1];
  char nick[NICKMAX + 1];

  // TODO: sjekke at hvert inputfelt ikke overstiger avsatt buffer

  int rc = sscanf(buffer, "PKT %8d %6s %20s\n", &sequence_number, message_type, nick);
  if (rc != 3) {
    check_error(-1, "Feil format på melding!");
  }

  printf("SEQ: %d\n", sequence_number);
  printf("type: %s\n", message_type);
  printf("nick: %s\n", nick);

  if(strcmp(message_type, "REG") == 0){
    register_client(nick);
    // TODO: send ack?
  }

  else if (strcmp(message_type, "LOOKUP") == 0){
    struct client *client = lookup_nick(nick);
    if(client != NULL){
      char return_msg[BUFMAX];

      sprintf(return_msg, "ACK %d NICK %s IP %s PORT %d\n", sequence_number, client->nick, (char *)client->ip, client->port);
      send_packet(sockfd, buffer, strlen(return_msg), 0, (struct sockaddr*)&clientaddr, sizeof(struct sockaddr_in));
    }
  }


  // TODO: slå opp client
}

/*
REGISTRERING AV CLIENT
*/

void register_client(char *nick){
  struct client *client = lookup_nick(nick);
  if (client == NULL){
    add_client(nick);

  }

  else {
        // oppdater socket
        client->ip = &clientaddr.sin_addr;
        client->port = clientaddr.sin_port;
        printf("client socket updated\n" );
  }

  // TODO: send ACK
}


/*
DATASTRUKTUR FOR CLIENT-LISTE OG TILHØRENDE METODER
*/


// legger client foran i listen
void add_client(char *nick){
  struct client *client = malloc(sizeof(*client));

  client->nick = malloc(NICKMAX);
  strcpy(client->nick, nick);

  client->ip = &clientaddr.sin_addr;
  client->port = clientaddr.sin_port;
  client->next = client_list;
  client_list = client;
  printf("client added to client list\n" );
}


void print_linked_list(struct client *client){

  printf("client: %s", client->nick);
  while(client->next != NULL){
    printf("->");
    print_linked_list(client->next);
  }
  printf("\n");
}

// itererer lenkeliste. Returnerer client hvis nick finnes, ellers null
struct client * lookup_nick(char *nick){

  struct client *client = client_list;
  if (client == NULL) {
    return NULL;
  }
  else if (client->nick == nick){
    return client;
  }
  else if(client->next == NULL){
    return NULL;
  }
  return lookup_client(client->next, nick);
}

struct client * lookup_client(struct client *client, char *nick){
  if (client->nick == nick){
    return client;
  }
  else {
    return NULL;
  }
}


// rekursiv free'ing av alle clients
void free_client(struct client *client){
  if (client == NULL) {
    return;
  }
  else if (client->next == NULL) {
    free(client->nick);
    free(client);
  }
  else {
    free_client(client->next);
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





