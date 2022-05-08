#include "send_packet.h"
#include "client_list.c"

#define MSG_MAX 1448 // 1400 + pluss litt ekstra
#define SERVER_MSG_MAX 48 // makslengde på meldinger til server, stemme roverens med BUFMAX i server


// METODER
void check_error(int res, char *msg);
void get_string(char str[], int size);
void send_REG_to_server(int sockfd);
struct client * lookup_client_in_server(int sequence_number, char *nick, int sockfd, fd_set fds, struct timeval timeout);

// GLOBALE VARIABLER
struct client * known_clients;  // cache med kjente klienter. lenkeliste.
struct sockaddr_in my_addr, serv_addr;


int main(int argc, char *argv[]){
  int sockfd, rc, timer;
  char *my_nick;
  char rcv_msg[MSG_MAX], stdin_msg[MSG_MAX], reg_msg[SERVER_MSG_MAX];
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


  // Setter variabler fra kommandolinje
  my_nick = argv[1];

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(atoi(argv[3]));
  rc = inet_pton(AF_INET, argv[4], &serv_addr.sin_addr.s_addr);
  check_error(rc, "inet_pton failed");
  set_loss_probability(atoi(argv[5]));

  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(0); // OS setter port selv
  my_addr.sin_addr.s_addr = INADDR_ANY; // OS finner IP automatisk
  rc = bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr_in));
  check_error(rc, "Failed to bind socket to sockfd");

  /*
  Setter kjent client for DEBUG
  */

  // known_clients = malloc(sizeof(known_clients));
  // char *friend_nick = "ola";
  // struct sockaddr_in friend_sockaddr;
  // friend_sockaddr.sin_port = htons(2222);
  // my_addr.sin_addr.s_addr = INADDR_ANY; // OS finner IP automatisk
  // known_clients = add_client(known_clients, friend_nick, &friend_sockaddr);

  /*
  */

  FD_ZERO(&fds); // initialiserer fds til en tom liste

  stdin_msg[0] = '\0';
  rcv_msg[0] = '\0';
  reg_msg[0] = '\0';

  // send REG melding til server
  // STOP-AND-WAIT VED REGISTRERING
  sprintf(reg_msg, "PKT %d REG %s\n", sequence_number, my_nick);
  FD_SET(sockfd, &fds);

  rc = send_packet(sockfd, reg_msg, strlen(reg_msg), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
  check_error(rc, "error when sending message");
  printf("REG message sent: %s\n", reg_msg);

  timeout.tv_sec = atoi(argv[4]);
  timeout.tv_usec = 0;
  timer = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);

  while (1){
    if (FD_ISSET(sockfd, &fds)) {
      char ack_ok[SERVER_MSG_MAX];
      rc = read(sockfd, rcv_msg, MSG_MAX - 1);
      check_error(rc, "read");
      rcv_msg[rc] = '\0';
      sprintf(ack_ok, "ACK %d OK", sequence_number);

      if(strcmp(ack_ok, rcv_msg) != 0){
        printf("Registration failed: wrong response from server\n");
        return EXIT_FAILURE;
      }

      printf("\rServer: %s\n", rcv_msg );
      break;
    }
    else if(timer <= 0){
      printf("Registration failed: no response from server\n" );
      return EXIT_FAILURE;
    }
  }


  // boilerplate fra Cbra
  while(1) {

      printf("Send message: ");
      fflush(NULL);
      FD_SET(STDIN_FILENO, &fds);
      FD_SET(sockfd, &fds);
      // TODO: uncomment timeout
      // timeout.tv_sec = atoi(argv[4]);
      // timeout.tv_usec = 0;


      timer = select(FD_SETSIZE, &fds, NULL, NULL, NULL); // TODO: legge til &timeout i siste param
      check_error(timer, "error when doing select");

      // TODO: en countdown som sjekker responstid og en for heartbeat?
      //if(rc == 0){
        // TODO: sende heartbeat til server
        //continue;
      //}

      if(FD_ISSET(STDIN_FILENO, &fds)) {
        get_string(stdin_msg, MSG_MAX);
        char to_nick[20];
        char text[1400];

        if (strcmp(stdin_msg, "QUIT") == 0) {
          break;
        }


        int rc = sscanf(stdin_msg, "@%s %1400s", to_nick, text);
        if (rc != 2) {
          printf("Wrong format: @<to_nick> <message>\n" );
          continue;
        }


        // GJØR OPPSLAG PÅ NICK
        struct client *to_client = lookup_client(known_clients, to_nick);

        if(to_client == NULL){
          to_client = lookup_client_in_server(sequence_number, to_nick, sockfd, fds, timeout);
          printf("port of %s is: %d\n", to_client->nick, to_client->sockaddr->sin_port );
        }

        if(to_client == NULL){
          printf("The client %s is unknow\n", to_nick);
        }
        else {
          rc = send_packet(sockfd, text, strlen(text), 0, (struct sockaddr *)to_client->sockaddr, sizeof(struct sockaddr));
          check_error(rc, "failed to send message");
          printf("packet sent to %s on port %d\n", to_client->nick, to_client->sockaddr->sin_port);
        }


      } else if (FD_ISSET(sockfd, &fds)) {
        rc = read(sockfd, rcv_msg, MSG_MAX - 1);
        check_error(rc, "read");
        rcv_msg[rc] = '\0';
        printf("\rMotatt melding:%s\n", rcv_msg); // TODO: parse ut nicket
      }
  }

  // Rydder opp minne og sockets:
  close(STDIN_FILENO);
  close(sockfd);
  free_clients(known_clients);

  return 0;
}

struct client * lookup_client_in_server(int sequence_number, char *nick, int sockfd, fd_set fds, struct timeval timeout){
  char lookup_msg[SERVER_MSG_MAX], lookup_response[SERVER_MSG_MAX], ack_nick[NICKMAX], ack_ip_s[16], ack_fail[SERVER_MSG_MAX];
  int ack_seq, ack_port, rc;
  struct sockaddr_in client_sockaddr;

  // formatterer lookup-request for å sende til server
  sprintf(lookup_msg, "PKT %d LOOKUP %s", sequence_number, nick);

  // sender lookup request
  rc = send_packet(sockfd, lookup_msg, strlen(lookup_msg), 0, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr));
  check_error(rc, "error sending message");

  // formatterer forventet feilmelding
  sprintf(ack_fail, "ACK %d NOT FOUND", sequence_number);

  // gjør oss klar til å lytte til udp-socket, samt setter timer
  FD_SET(sockfd, &fds);
  int timer = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);

  while (1){
    if (FD_ISSET(sockfd, &fds)) {
      rc = read(sockfd, lookup_response, SERVER_MSG_MAX - 1);
      check_error(rc, "read");
      lookup_response[rc] = '\0';
      printf("we received something: %s\n", lookup_response );

      rc = sscanf(lookup_response, "ACK %d NICK %s IP %s PORT %d", &ack_seq, ack_nick, ack_ip_s, &ack_port);
      printf("the port of %s is %d\n", ack_nick, ack_port );

      // Hvis nick ikke er hos serve, bør respons matche forventet feilmelding
      // TODO: drite i all sjekkinga og bare se på ack_seq?
      if(strcmp(lookup_response, ack_fail) == 0){
        fprintf(stderr, "NICK %s NOT REGISTERED\n", nick);
        return NULL;
      }

      // unødvendig sjekk? Gjelder bare hvis en pakke blir ødelagt på veien, men fortsatt leveres
      if(rc != 4){
        fprintf(stderr, "received wrong format on lookup response: %s\n", lookup_response);
      }

      // respons var bra, legger til client i known_clients:
      known_clients = add_client(known_clients, nick, ack_port, ack_ip_s);

      return known_clients;

    }

    if(timer <= 0){
      printf("Lookup failed: no response from server\n" );
      exit(EXIT_FAILURE);
    }
  }
  return NULL;
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

    /* fjern newline fra slutten av strengen */
    if (str[strlen(str) - 1] == '\n') {
        str[strlen(str) - 1] = '\0';
    }
    /* Fjern resten av stdin dersom mer enn BUFSIZE ble lest inn */
    else while ((c = getchar()) != '\n' && c != EOF);
}