#include "send_packet.h"
#include <ctype.h>

#define BUFMAX 48 // makslengde på melding fra klient vil aldri overstige dette
#define SEQMAX 8 // maks antall bytes vi tillater på sekvensnummer
#define NICKMAX 20 //makslengde vi tillater på nick
#define MESSAGE_TYPE_MAX 6 // makslengde på meldingstype som er lengden til LOOKUP

int PORT;
int LOSS_PROBABILITY;

struct sockaddr_in clientaddr;
struct sockaddr_in serveraddr;


// HJELPEMETODER
int check_char(char c);
void check_error(int res, char *msg);
void parse_message(char *buf);

// GLOBALE VARIABLER
struct client *client_list;

int main(int argc, char *argv[]) {

    PORT = atoi(argv[1]);
    LOSS_PROBABILITY = atoi(argv[2]);


    // TODO: feilmelding dersom meldign er for lang?
    char buffer[BUFMAX] = {0};
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

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
        fprintf(stderr, "Failed to bind");
        exit(EXIT_FAILURE);
    }

    socklen_t clientaddr_len = sizeof(struct sockaddr_in);

    recvfrom(
        sockfd,
        buffer,
        BUFMAX,
        0,
        (struct sockaddr*)&clientaddr,
        &clientaddr_len); //Mottar melding fra client, lagrer addressen i clientaddr

    printf("Message received: %s\n", buffer); //Se om vi fikk meldingen.
    parse_message(buffer);

    close(sockfd);
    return EXIT_SUCCESS;
}

/*
PARSE MELDING FRA CLIENT
*/

void parse_message(char *buffer){

  int sequence_number;
  char message_type[MESSAGE_TYPE_MAX + 1];
  char nick[NICKMAX + 1];

  // TODO: sjekke at hvert inputfelt ikke overstoger avsatt buffer
  int rc = sscanf(buffer, "PKT %8d %6s %19s\n", &sequence_number, message_type, nick);
  if (rc != 3) {
    check_error(-1, "Feil format på melding!");
  }

  printf("SEQ: %d\n", sequence_number );
  printf("type: %s\n", message_type );
  printf("nick: %s\n", nick );

  // TODO: registrer client
  // TODO: slå opp client
}

/*
REGISTRERING AV CLIENT
*/

void register_client(char *nick){
  // TODO: sjekk om bruker finnes fra før
  // TODO: legg til i lenkeliste
  // TOCO: send ACK
}


/*
DATASTRUKTUR FOR CLIENT-LISTE
*/

typedef struct client { // client-struct for enkel-lenket lenkeliste
  char *nick;
  struct sockaddr_in addr;
  struct client *next;
};

void add_client(char *nick){ // legger client foran i listen
  struct client *client = malloc(sizeof(client));
  client->nick = nick;
  client->addr = clientaddr; // global variabel
  client->next = client_list;
  client_list = client;
}

void print_linked_list(){
  struct client *client = client_list;

  while(client->next != NULL){
    printf("%s -> ", client->nick);
  }
  fflush(stdout);
}


/*
HJELPEMETODER
*/

int check_char(char c){
  if (!isalpha(c) && !isalnum(c) && !isspace(c)) {
    printf("illegal character is: %s\n", c);
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





