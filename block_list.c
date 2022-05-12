
#define NICKMAX 20 //makslengde vi tillater på nick



typedef struct blocked_nick{
  char *nick;
  struct blocked_nick *next;
} blocked_nick;

void print_block_list(struct blocked_nick *blocked_nick);


struct blocked_nick * block_nick(struct blocked_nick * block_list, char *nick){
  struct blocked_nick * blocked_nick = malloc(sizeof(struct blocked_nick));

  blocked_nick->nick = malloc(NICKMAX);
  strcpy(blocked_nick->nick, nick);
  blocked_nick->next = block_list;
  printf("%s is blocked\n", nick);
  print_block_list(block_list);
  return blocked_nick;

}

struct blocked_nick * unblock_nick(struct blocked_nick * blocked_nick, char *nick){

  if (blocked_nick == NULL) {
    return NULL;
  }

  if(blocked_nick->next == NULL){
    return NULL;
  }

  // dersom neste sitt nick er riktig
  if (strcmp(blocked_nick->next->nick, nick) == 0) {
    blocked_nick->next = blocked_nick->next->next;
  }

  unblock_nick(blocked_nick->next, nick);

  return blocked_nick;
}

void print_block_list(struct blocked_nick *blocked_nick){
  if (blocked_nick == NULL) {
    return;
  }
  printf("blocked nick: %s", blocked_nick->nick);
  while(blocked_nick->next != NULL){
    printf(" -> ");
    blocked_nick = blocked_nick->next;
    printf("%s", blocked_nick->nick );
  }
  printf("\n");
}

// itererer lenkeliste. Returnerer blocked_nick hvis nick finnes, ellers null
struct blocked_nick * lookup_blocked_nick(struct blocked_nick *blocked_nick, char *nick){
  print_block_list(blocked_nick);

  if (blocked_nick == NULL) {
    return NULL;
  }
  else if (strcmp(blocked_nick->nick, nick) == 0){
    return blocked_nick;
  }
  else if(blocked_nick->next == NULL){
    return NULL;
  }
  return lookup_blocked_nick(blocked_nick->next, nick);
}


// rekursiv free'ing av alle blocked_nicks
void free_blocked_nicks(struct blocked_nick *blocked_nick){
  if (blocked_nick == NULL) {
    return;
  }
  else if (blocked_nick->next == NULL) {
    free(blocked_nick->nick);
    free(blocked_nick);
  }
  else {
    free_blocked_nicks(blocked_nick->next);
  }
}