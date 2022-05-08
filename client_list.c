#include "send_packet.h"

#define NICKMAX 20 //makslengde vi tillater på nick



typedef struct client{
  char *nick;
  struct sockaddr_in *sockaddr;
  struct client *next;
  int last_message; // meldingsnummer på siste melding vi mottok. Klienter bruker for å unngå duplikater
  // todo: lenkeliste over utgående meldinger?
} client;


/*
DATASTRUKTUR FOR CLIENT-LISTE OG TILHØRENDE METODER
*/


// legger client foran i listen
struct client * add_client(struct client *client_list, char *nick, int port, in_addr_t ip){
  struct client *client = malloc(sizeof(struct client));

  // lagrer nick
  client->nick = malloc(NICKMAX);
  strcpy(client->nick, nick);

  // lagrer sockaddr
  client->sockaddr = malloc(sizeof(struct sockaddr_in));
  client->sockaddr->sin_family = AF_INET;
  client->sockaddr->sin_port = port;
  client->sockaddr->sin_addr.s_addr = ip;
  //inet_pton(AF_INET, ip_string, &client->sockaddr->sin_addr.s_addr);

  // legger client foran i lenkelisten
  client->next = client_list;
  client->last_message = 0;

  return client;
}


void print_linked_list(struct client *client){
  if (client == NULL) {
    return;
  }
  printf("client: %s", client->nick);
  while(client->next != NULL){
    printf(" -> ");
    client = client->next;
    printf("%s", client->nick );
  }
  printf("\n");
}

// itererer lenkeliste. Returnerer client hvis nick finnes, ellers null
struct client * lookup_client(struct client *client, char *nick){
  printf("lookup client %s\n", nick );
  printf("in this linke dlist:\n" );
  print_linked_list(client);

  if (client == NULL) {
    return NULL;
  }
  else if (strcmp(client->nick, nick) == 0){
    return client;
  }
  else if(client->next == NULL){
    return NULL;
  }
  return lookup_client(client->next, nick);
}


// rekursiv free'ing av alle clients
void free_clients(struct client *client){
  if (client == NULL) {
    return;
  }
  else if (client->next == NULL) {
    free(client->nick);
    free(client->sockaddr);
    free(client);
  }
  else {
    free_clients(client->next);
  }
}