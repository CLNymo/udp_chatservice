#include "send_packet.h"
#include "client_list.c"
#include <time.h>

#define TEXT_MAX 1400
#define MSG_MAX 1448 // 1400 + pluss litt ekstra
#define SERVER_MSG_MAX 48 // makslengde på meldinger til server, stemme roverens med BUFMAX i server
#define HEADER_MAX 70 // makslengde på meldings-header


// METODER
void check_error(int res, char *msg);
void get_string(char str[], int size);
void send_REG_to_server(int sockfd);
int lookup_client_in_server(int sequence_number, char *nick, int sockfd, fd_set fds, int timeout);

// GLOBALE VARIABLER
struct client * known_clients;  // cache med kjente klienter. lenkeliste.
struct sockaddr_in my_addr, serv_addr;


int main(int argc, char *argv[]){
  int sockfd, rc, select_timer;
  char *my_nick;
  char msg_rcv[MSG_MAX], stdin_msg[MSG_MAX], msg_reg[SERVER_MSG_MAX];
  fd_set fds;
  struct timeval timeout;


  int sequence_number = 0; // TODO: wraparound hvis overstiger 8 bits

  if (argc < 5){
    printf("usage: %s <nick> <adresse> <port> <timeout> <tapssannsynlighet>\n", argv[0]);
    return EXIT_SUCCESS;
  }

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  check_error(sockfd, "failed to create socket");

  memset(&serv_addr, 0, sizeof(serv_addr));


  // Setter min addresse:
  my_nick = argv[1];
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(0); // OS velger port selv
  my_addr.sin_addr.s_addr = INADDR_ANY; // OS finner IP automatisk
  rc = bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr_in));
  check_error(rc, "Failed to bind socket to sockfd");

  // setter server addresse:
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(atoi(argv[3]));
  rc = inet_pton(AF_INET, argv[4], &serv_addr.sin_addr.s_addr);
  check_error(rc, "inet_pton failed");

  srand48(time(NULL));
  set_loss_probability(atof(argv[5])/100);

  // initialiserer meldinger
  stdin_msg[0] = '\0';
  msg_rcv[0] = '\0';
  msg_reg[0] = '\0';

  // send REG melding til server
  sprintf(msg_reg, "PKT %d REG %s\n", sequence_number, my_nick);
  rc = send_packet(sockfd, msg_reg, strlen(msg_reg), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
  check_error(rc, "error when sending message");
  printf("REG message sent: %s\n", msg_reg);


  // STOP-AND-WAIT for REG-melding
  FD_ZERO(&fds);
  FD_SET(sockfd, &fds);
  timeout.tv_sec = atoi(argv[4]);
  timeout.tv_usec = 0;
  select_timer = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);

  if (FD_ISSET(sockfd, &fds)) {
    char ack_ok[SERVER_MSG_MAX];

    rc = read(sockfd, msg_rcv, SERVER_MSG_MAX - 1);
    check_error(rc, "read");
    msg_rcv[rc] = '\0';
    sprintf(ack_ok, "ACK %d OK", sequence_number);

    if(strcmp(ack_ok, msg_rcv) != 0){
      printf("Registration failed: wrong response from server\n");
      return EXIT_FAILURE;
    }

    printf("\rServer: %s\n", msg_rcv );
  }

  else {
    printf("Registration failed: no response from server\n" );
    return EXIT_FAILURE;
  }


  // boilerplate fra Cbra
  // Skal bli Stop and wait for stdin og sockfd
  while(1) {
    int res;
    FD_ZERO(&fds); // initialiserer fds til en tom liste
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(sockfd, &fds);

    printf("Send message: ");
    fflush(NULL);

    timeout.tv_sec = atoi(argv[4]);
    timeout.tv_usec = 0;


    select_timer = select(FD_SETSIZE, &fds, NULL, NULL, NULL); // TODO: sett &timeout på siste
    check_error(select_timer, "error when doing select");

    if(FD_ISSET(STDIN_FILENO, &fds)) {
      char to_nick[20], text[TEXT_MAX], msg_send[MSG_MAX];

      // les fra stdin
      get_string(stdin_msg, MSG_MAX);

    // sjekker input
      if (strcmp(stdin_msg, "QUIT") == 0) {
        break;
      }

      res = sscanf(stdin_msg, "@%s %s", to_nick, text);

    // input ikke OK, prøv igjen
      if (res != 2) {
        printf("Wrong format: @<to_nick> <message>\n" );
        continue;
      }

    // input OK
      // parser ut selve teksten
      strcpy(text, &stdin_msg[strlen(to_nick) + 2]); // +2 for @ før to_nick og space etter to_nick
      //text[TEXT_MAX - 1] = '\0';

      // formatterer meldings-pakken
      sprintf(msg_send, "PKT %d FROM %s TO %s MSG %s", sequence_number, my_nick, to_nick, text);

      // oppslag på nick i cache
      struct client *to_client = lookup_client(known_clients, to_nick);

      // Hvis lokalt oppslag OK, send pakke
      if(to_client != NULL){
        res = send_packet(sockfd, msg_send, strlen(msg_send), 0, (struct sockaddr *)to_client->sockaddr, sizeof(struct sockaddr));
        check_error(rc, "failed to send message");
        printf("packet sent to %s on addr %d port %d\n", to_client->nick, to_client->sockaddr->sin_addr.s_addr, to_client->sockaddr->sin_port);
        continue;
      }

      // Hvis lokalt oppslag ikke OK, utfør oppslag i server
      res = lookup_client_in_server(sequence_number, to_nick, sockfd, fds, atoi(argv[4]));

      // Hvis lookup i server returnerte NOT FOUND
      if(res == -1){
        printf("Your friend %s doesn't exist on the server\n", to_nick);
        continue;
      }

      // Hvis server ikke svarte på lookup
      else if (res == -2) {
        printf("No response from server: quitting program.\n" );
        break;
      }

      // Hvis lookup i server var OK, send pakke til client som nå ligger i cache known_clients
      to_client = lookup_client(known_clients, to_nick);
      res = send_packet(sockfd, msg_send, strlen(msg_send), 0, (struct sockaddr *)to_client->sockaddr, sizeof(struct sockaddr));
      check_error(rc, "failed to send message");
      printf("packet may be sent to %s on addr %d port %d\n", known_clients->nick, to_client->sockaddr->sin_addr.s_addr, to_client->sockaddr->sin_port);

    }

    // Leser fra socket
    else if (FD_ISSET(sockfd, &fds)) {

      int seq = 0;
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

      // Sjekker om meldingen vi mottok var en ack:
      if(sscanf(msg_rcv, "ACK %d", &seq) == 1){
        printf("Vi mottok en ack, som vi ignorerer for øyeblikket: %s\n", msg_rcv);
        continue;
      }


      res = sscanf(msg_rcv, "PKT %d FROM %s TO %s MSG ", &seq, from_nick, to_nick);
      check_error(res, "Unable to scan incoming message");

      // Verifisere at melding har riktig format
      if (res != 3) {
        fprintf(stderr, "Failed to interpret incoming message, format is probably wrong: %s\n", msg_rcv);
        sprintf(ack, "ACK %d WRONG FORMAT", seq);
      }

      // Verifiserer at melding har riktig to_nick
      else if (strcmp(to_nick, my_nick) != 0) {
        fprintf(stderr, "Incoming message sent to wrong nick\n");
        sprintf(ack, "ACK %d WRONG NAME", seq);
      }

      // Melding er OK
      else {
        sprintf(ack, "ACK %d OK", seq);
      }

      // Fjerne header fra melding før vi skriver ut til stdout
      char msg_header[HEADER_MAX];
      sprintf(msg_header, "PKT %d FROM %s TO %s MSG ", seq, from_nick, to_nick);
      strcpy(text, &msg_rcv[strlen(msg_header)]);

      // Skriver text til stdout
      printf("\r%s: %s\n", from_nick, text);

      // Sender ACK'en
      printf("Sender ack: %s\n", ack);
      send_packet(sockfd, ack, strlen(ack), 0, &to_addr, sizeof(struct sockaddr));
    }
  }

  // Rydder opp minne og sockets:
  close(STDIN_FILENO);
  close(sockfd);
  free_clients(known_clients);

  return 0;
}

int lookup_client_in_server(int sequence_number, char *nick, int sockfd, fd_set fds, int timeout_seconds){
  char lookup_msg[SERVER_MSG_MAX], lookup_response[SERVER_MSG_MAX], ack_nick[NICKMAX], ack_ip_s[16], ack_fail[SERVER_MSG_MAX];
  int ack_seq, ack_port, rc, tries, select_timer;
  in_addr_t ack_ip;
  struct sockaddr_in client_sockaddr;
  struct timeval timeout;

  // formatterer lookup-request for å sende til server
  sprintf(lookup_msg, "PKT %d LOOKUP %s", sequence_number, nick);

  // sender lookup request

  // formatterer forventet feilmelding
  sprintf(ack_fail, "ACK %d NOT FOUND", sequence_number);

  tries = 0;

  while (1){
    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);

    rc = send_packet(sockfd, lookup_msg, strlen(lookup_msg), 0, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
    check_error(rc, "error sending message");

    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;

    select_timer = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
    check_error(select_timer, "Error when doing select in lookup");

    if (FD_ISSET(sockfd, &fds)) {
      rc = read(sockfd, lookup_response, SERVER_MSG_MAX - 1);
      check_error(rc, "read");
      lookup_response[rc] = '\0';
      printf("we received something: %s\n", lookup_response );

      rc = sscanf(lookup_response, "ACK %d NICK %s IP %s PORT %d", &ack_seq, ack_nick, ack_ip_s, &ack_port);

      // Sjekker om respons var en feilmelding:
      if(strcmp(lookup_response, ack_fail) == 0){
        fprintf(stderr, "NICK %s NOT REGISTERED\n", nick);
        return -1;

      // Respons var bra, legger til client foran i known_clients:
      } else {
        known_clients = add_client(known_clients, nick, ack_port, inet_addr(ack_ip_s));
        return 0;
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

    // Sjekk dersom en pakke blir ødelagt på veien, men fortsatt leveres
    // if(rc != 4){
    //   fprintf(stderr, "Received wrong format on lookup response: %s\n", lookup_response);
    // }
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