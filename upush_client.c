#include "send_packet.h"

void check_error(int res, char *msg){ // greit å ha siden mange metoder returnerer -1 ved error
  if (res == -1){
    perror(msg);
    // Rydde?
    exit(EXIT_FAILURE);
  }
}

int main()
{
  char * hello = "PKT 666666666666666 REG Nicolasssssssssss";

  struct sockaddr_in servaddr = {0}; // curly loader på stack men nuller det ut så vi slipper memset

  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  check_error(sockfd, "failed to create socket");


  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(12345);
  servaddr.sin_addr.s_addr = INADDR_ANY;

  int len = sendto(sockfd, (const char * ) hello, strlen(hello), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
  if (len == -1)
  {
    perror("failed to send");
  }
  close(sockfd);

  return 0;
}