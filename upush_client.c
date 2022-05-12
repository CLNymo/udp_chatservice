#include "send_packet.h"
#include "client_list.c"
#include "block_list.c"
#include <time.h>

#define TEXT_MAX 1400
#define MSG_MAX 1448 // 1400 + pluss litt ekstra
#define SERVER_MSG_MAX 48 // makslengde på meldinger til server, stemme roverens med BUFMAX i server
#define HEADER_MAX 70 // makslengde på meldings-header


// METODER
void check_error(int res, char *msg);
void get_string(char str[], int size);
void send_REG_to_server(int sockfd);
int lookup_client_in_server(char *nick);
int await_ack_from_client(client *client);
void send_packet_to_client(struct client *client, char *msg);


// GLOBALE VARIABLER
struct client * known_clients;
struct blocked_nick * blocked_nicks;
struct sockaddr_in my_addr, serv_addr;
int sockfd;
int timeout_seconds;
int my_seq_number, serv_seq_number; // to ulike sekvensnumre, fordi vi noen ganger venter på ACK fra både klient og server samtidig.
fd_set fds;
struct timeval timeout;


int main(int argc, char *argv[]){
  int res, select_res;
  char *my_nick;
  char msg_rcv[MSG_MAX], stdin_msg[MSG_MAX], msg_reg[SERVER_MSG_MAX];
  my_seq_number, serv_seq_number = 0;


  if (argc < 5){
    printf("usage: %s <nick> <adresse> <port> <timeout> <tapssannsynlighet>\n", argv[0]);
    return EXIT_SUCCESS;
  }

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  check_error(sockfd, "failed to create socket");

  memset(&serv_addr, 0, sizeof(serv_addr));

  timeout_seconds = atoi(argv[4]);


  // Setter min addresse:
  my_nick = argv[1];
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(0); // OS velger port selv
  my_addr.sin_addr.s_addr = INADDR_ANY; // OS finner IP automatisk
  res= bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr_in));
  check_error(res, "Failed to bind socket to sockfd");

  // setter server addresse:
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(atoi(argv[3]));
  res= inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
  check_error(res, "inet_pton failed");

  srand48(time(NULL));
  set_loss_probability(atof(argv[5])/100);

  // initialiserer meldinger
  stdin_msg[0] = '\0';
  msg_rcv[0] = '\0';
  msg_reg[0] = '\0';

  // send REG melding til server
  sprintf(msg_reg, "PKT %d REG %s\n", serv_seq_number, my_nick);
  res = send_packet(sockfd, msg_reg, strlen(msg_reg), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
  check_error(res, "error when sending message");
  printf("REG message sent: %s\n", msg_reg);


  // STOP-AND-WAIT for REG-melding
  FD_ZERO(&fds);
  FD_SET(sockfd, &fds);
  timeout.tv_sec = atoi(argv[4]);
  timeout.tv_usec = 0;
  select_res = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);

  if (FD_ISSET(sockfd, &fds)) {
    char ack_ok[SERVER_MSG_MAX];

    res = read(sockfd, msg_rcv, SERVER_MSG_MAX - 1);
    check_error(res, "read");
    msg_rcv[res] = '\0';

    // Formatterer ønsket ack og ser om vi mottok riktig
    sprintf(ack_ok, "ACK %d OK", serv_seq_number);
    if(strcmp(ack_ok, msg_rcv) != 0){
      printf("Registration failed: wrong response from server\n");
      return EXIT_FAILURE;
    }

    printf("\rServer: %s\n", msg_rcv);
  }

  else {
    printf("Registration failed: no response from server\n" );
    return EXIT_FAILURE;
  }


  // NB! Mye boilerplate fra Cbra
  // Stop and wait for stdin og sockfd
  while(1) {
    int res;

    FD_ZERO(&fds); // initialiserer fds til en tom liste
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(sockfd, &fds);

    timeout.tv_sec = atoi(argv[4]);
    timeout.tv_usec = 0;

    select_res = select(FD_SETSIZE, &fds, NULL, NULL, NULL); // TODO: sett &timeout på siste
    check_error(select_res, "error when doing select");

    if(FD_ISSET(STDIN_FILENO, &fds)) {
      char to_nick[20], text[TEXT_MAX], msg_send[MSG_MAX];

      // les fra stdin
      get_string(stdin_msg, MSG_MAX);

      // sjekker input
      if (strcmp(stdin_msg, "QUIT") == 0) {
        break;
      }

      // Sjekker om block-melding
      if(sscanf(stdin_msg, "BLOCK %s", to_nick) == 1){
        printf("Received BLOCK message\n" );
        // Sjekker om nick er blokkert fra før
        if (lookup_blocked_nick(blocked_nicks, to_nick) == NULL) {
          blocked_nicks = block_nick(blocked_nicks, to_nick);
          print_block_list(blocked_nicks);
        }
        continue;
      }

      // Sjekker om unblock-melding
      if(sscanf(stdin_msg, "UNBLOCK %s", to_nick) == 1){
        printf("Received unblock message\n" );
        blocked_nicks = unblock_nick(blocked_nicks, to_nick);
        print_block_list(blocked_nicks);
        continue;
      }

      // input ikke OK, prøv igjen
      if (sscanf(stdin_msg, "@%s %s", to_nick, text) != 2) {
        printf("Wrong format: @<to_nick> <message>\n" );
        continue;
      }

      /* INPUT OK */

      // sjekker om vi prøver å sende til oss selv
      if (strcmp(to_nick, my_nick) == 0) {
        continue;
      }

      // sjekker om nick er blokkert
      if (lookup_blocked_nick(blocked_nicks, to_nick) != NULL) {
        printf("Prøvde å sende melding til blokkert nick\n" );
        continue;
      }

      // TODO: flytt i egen metode?!
      // lokalt oppslag på nick i cache
      struct client *to_client = lookup_client(known_clients, to_nick);

      // Hvis lokalt oppslag ikke OK, utfør oppslag i server
      if(to_client == NULL){
        res = lookup_client_in_server(to_nick);

        // Hvis lookup i server var OK kan vi nå finne client i known_clients
        if (res == 0){
          to_client = lookup_client(known_clients, to_nick);
        }

        // Hvis lookup i server returnerte NOT FOUND
        else if(res == -1){
          printf("Your friend %s doesn't exist on the server\n", to_nick);
          continue;
        }

        // Hvis server ikke svarte på lookup
        else {
          printf("No response from server: quitting program.\n" );
          break;
        }

      }

      // parser ut teksten som skal sendes fra stdin
      strcpy(text, &stdin_msg[strlen(to_nick) + 2]); // +2 for @ før to_nick og space etter to_nick

      // formatterer meldings-pakken
      my_seq_number ++;
      sprintf(msg_send, "PKT %d FROM %s TO %s MSG %s", my_seq_number, my_nick, to_nick, text);
      printf("package createD: %s\n", msg_send);

      // sender pakken i egen metode
      send_packet_to_client(to_client, msg_send);
    }


    // Leser fra socket
    else if (FD_ISSET(sockfd, &fds)) {

      // read_msg_from_client();

      int seq_incoming;
      struct sockaddr to_addr;
      char from_nick[20], to_nick[20], text[TEXT_MAX];
      char ack[SERVER_MSG_MAX];
      socklen_t to_addr_len = sizeof(struct sockaddr);

      msg_rcv[0] = '\0';
      ack[0] = '\0';

      res = recvfrom(
        sockfd,
        msg_rcv,
        MSG_MAX,
        0,
        (struct sockaddr*)&to_addr,
        &to_addr_len
      );

      msg_rcv[res] = '\0';


      // Tolker innkommende melding
      res = sscanf(msg_rcv, "PKT %d FROM %s TO %s MSG ", &seq_incoming, from_nick, to_nick);
      check_error(res, "Unable to scan incoming message");

      // Verifisere at melding har riktig format
      if (res != 3) {
        fprintf(stderr, "Failed to interpret incoming message, format is probably wrong: %s\n", msg_rcv);
        sprintf(ack, "ACK %d WRONG FORMAT", seq_incoming);

        // Hvis pakken har feil format, sender vi ACK uten å skrive til stdout
        send_packet(sockfd, ack, strlen(ack), 0, &to_addr, sizeof(struct sockaddr));
        continue;
      }

      // Verifiserer at melding har riktig to_nick
      else if (strcmp(to_nick, my_nick) != 0) {
        fprintf(stderr, "Incoming message sent to wrong nick\n");
        sprintf(ack, "ACK %d WRONG NAME", seq_incoming);

        // Hvis pakken har feil navn, sender vi ACK uten å skrive til stdout
        send_packet(sockfd, ack, strlen(ack), 0, &to_addr, sizeof(struct sockaddr));
        continue;
      }

      /* MELDING ER OK */


      sprintf(ack, "ACK %d OK", seq_incoming);


      // Sjekk om avsender er blokkert
      if (lookup_blocked_nick(blocked_nicks, from_nick) != NULL) {
        printf("Mottok melding fra blokkert nick\n" );
        continue;
      }

      // Sender ACK'en
      printf("Sender ack: %s\n", ack);
      send_packet(sockfd, ack, strlen(ack), 0, &to_addr, sizeof(struct sockaddr));


      // Sjekk om meldingen er duplikat (sjekk sekvensnummer i known_clients)
      struct client *from_client = lookup_client(known_clients, from_nick);

      if (from_client == NULL) {
        res = lookup_client_in_server(from_nick);
        if (res == 0){
          from_client = lookup_client(known_clients, from_nick);

        }
          // Hvis lookup i server returnerte NOT FOUND
        else if(res == -1){
          printf("A ghost tried to message you ۹(ÒہÓ)۶ \n");
          continue;
        }

        // Hvis server ikke svarte på lookup
        else {
          printf("No response from server: quitting program.\n" );
          break;
        }
      }

      // Hvis vi har mottatt meldingen før, ikke skriv til stdout
      if (from_client->sequence_number >= seq_incoming ) {
        continue;
      }

      // Hvis vi ikke har mottatt melding før, fjern header og skriv til stdout
      char msg_header[HEADER_MAX];
      sprintf(msg_header, "PKT %d FROM %s TO %s MSG ", seq_incoming, from_nick, to_nick);
      strcpy(text, &msg_rcv[strlen(msg_header)]);

      // opdatert sequence_number
      from_client->sequence_number = seq_incoming;

      // Skriver tekst til stdout
      printf("%s: %s\n", from_nick, text);
    }
  }

  // Rydder opp minne og sockets:
  close(STDIN_FILENO);
  close(sockfd);
  free_clients(known_clients);

  return 0;
}

void send_packet_to_client(struct client *client, char *msg){
  char *nick = client->nick;
  int res, attempts = 0, lookup_done = 0;

  while(!(attempts >= 2 && lookup_done == 1)){

    if (attempts >= 2) {

      // Hvis nytt oppslag feiler, gå ut av loop
      if (lookup_client_in_server(nick) != 0) {
        return;
      }

      // Hvis oppslag er vellykket, tillat to forsøk til
      client = lookup_client(known_clients, nick);
      attempts = 0;
      lookup_done = 1;

    }

    // Prøv å sende pakke
    res = send_packet(sockfd, msg, strlen(msg), 0, (struct sockaddr *)client->sockaddr, sizeof(struct sockaddr));
    check_error(res, "failed to send message");
    printf("packet may be sent to %s on addr %d port %d\n", client->nick, client->sockaddr->sin_addr.s_addr, client->sockaddr->sin_port);

    // Hvis ACK ble mottatt, gå ut av loop
    if(await_ack_from_client(client) == 0){
      return;
    }

    attempts ++;
  }

  // Hvis vi aldri mottok ACK
  fprintf(stderr, "NICK %s UNREACHABLE\n", nick);
}

int lookup_client_in_server(char *nick){
  char lookup_msg[SERVER_MSG_MAX], lookup_response[SERVER_MSG_MAX], ack_nick[NICKMAX], ack_ip_s[16], ack_fail[SERVER_MSG_MAX];
  int ack_seq, ack_port, res, tries, select_res;
  struct sockaddr_in client_sockaddr;

  int sequence_number = 0;

  // formatterer lookup-request for å sende til server
  serv_seq_number ++;
  sprintf(lookup_msg, "PKT %d LOOKUP %s", serv_seq_number, nick);


  // formatterer forventet feilmelding
  sprintf(ack_fail, "ACK %d NOT FOUND", serv_seq_number);

  tries = 0;

  while (1){
    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);

    // sender lookup request
    res = send_packet(sockfd, lookup_msg, strlen(lookup_msg), 0, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
    check_error(res, "error sending message");

    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;

    select_res = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
    check_error(select_res, "Error when doing select in lookup");

    if (FD_ISSET(sockfd, &fds)) {
      res = read(sockfd, lookup_response, SERVER_MSG_MAX - 1);
      check_error(res, "read");
      lookup_response[res] = '\0';
      printf("we received something: %s\n", lookup_response );

      res = sscanf(lookup_response, "ACK %d NICK %s IP %s PORT %d", &ack_seq, ack_nick, ack_ip_s, &ack_port);

      // Respons var som forventet, legger til client foran i known_clients:
      if (res == 4) {
        known_clients = add_client(known_clients, nick, ack_port, inet_addr(ack_ip_s));
        return 0;
      }

      // Respons ikke som forventet, sjekker om respons var en feilmelding:
      if(strcmp(lookup_response, ack_fail) == 0){
        fprintf(stderr, "NICK %s NOT REGISTERED\n", nick);
        return -1;

      // Melding kan ikke tolkes, skyldes trolig at en annen client vil sende melding imens vi venter på server. Ønsker ikke å telle det som en timeout.
      } else {
        printf("Ignoring a message because we're waiting for ACK from server: %s\n", lookup_response);
      }
    }

    // Timeren har gått ut
    else {
      printf("Nådde en timeout.\n");
      tries ++;

      if(tries >= 3){ // vi gir opp
        fprintf(stderr, "Lookup failed: no response from server\n" );
        return -2;

      } else { // vi prøver igjen
        printf("prøver igjen!\n");
        continue;
      }
    }
  }
}

// Hvis returnerer 0, så kom riktig ack. Hvis returnerer -1 så kom ikke ack før timeout.
int await_ack_from_client(client *client){
  int select_res, res;
  char msg_rcv[SERVER_MSG_MAX];
  socklen_t sockaddr_len = sizeof(struct sockaddr);
  int ack_sequence_number; // tallet vi mottar

  msg_rcv[0] = '\0';


  while(1){
    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);

    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;

    select_res = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
    check_error(select_res, "Error when doing select in await_ack_from_client");

    if(FD_ISSET(sockfd, &fds)){
      res = recvfrom(
        sockfd,
        msg_rcv,
        MSG_MAX,
        0,
        (struct sockaddr*)client->sockaddr,
        &sockaddr_len
      );

      msg_rcv[res] = '\0';

      // Sjekker om meldingen vi mottok var en ack:
      if(sscanf(msg_rcv, "ACK %d OK", &ack_sequence_number) == 1){
        if(ack_sequence_number == my_seq_number){
          printf("Vi mottok en ACK OK: %s\n", msg_rcv);
          return 0;
        }
        else {
          printf("Vi mottok en ACK med feil nummer: %s\n", msg_rcv);
        }
      }

      else {
        printf("Mottok en melding, men det var ikke ACK OK: %s\n", msg_rcv);
        //return -2;
      }
    }

    if (select_res == 0) {
      printf("Nådde en timeout.\n");
      return -1;
    }

  }

}

// HJELPEMETODER
void check_error(int res, char *msg){ // greit å ha siden mange metoder returnerer -1 ved error
  if (res == -1){
    perror(msg);
    // Rydde?
    exit(EXIT_FAILURE);
  }
}

void get_string(char str[], int size) {
    char c;
    fgets(str, size, stdin);

    // Fjern newline fra slutten av strengen
    if (str[strlen(str) - 1] == '\n') {
        str[strlen(str) - 1] = '\0';
    }
    // Fjern resten av stdin dersom mer enn BUFSIZE ble lest inn
    else while ((c = getchar()) != '\n' && c != EOF);
}