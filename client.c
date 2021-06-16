// client.c
// Client code of a chatroom.
// author: Josef E. Zerpa R.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFF_SIZE 256
#define TIMEOUT 5
#define handle_err(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

int QUIT;

void quit (int signo) {
  if (!QUIT) { return; } // quit() already run

  QUIT = 1;
}

int main (int argc, char **argv) {
  int connfd, portno, n;
  struct sockaddr_in serv_addr;
  char buff[BUFF_SIZE];
  struct sigaction action;
  struct timeval timeout;
  fd_set writefds;

  // Check options.
  if (argc != 3) {
	  printf("\nUsage: %s <server ip> <port>", argv[0]);
	  exit(1);
  }

  // Set up connection.
  if ( (connfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    handle_err("Error opening socket.");
  }
  if ( (portno = atoi(argv[2])) == 0) { 
    handle_err("Error parsing port number."); 
  }
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(portno);
  if ( (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr)) <= 0) {
    handle_err("Error parsing server address.");
  }
  if( (connect(connfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr))) < 0) {
    handle_err("Error trying to connect.");
  }

  
  // Spawn writer thread.
  // TODO: writer thread.
  
  // Sender thread.
  // Exit when SIGINT is caught.
  action.sa_handler = &quit;
  action.sa_flags = SA_RESETHAND; // reset to default after use
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGPIPE, &action, NULL);

  printf("Connected to %s.\n", argv[1]);
  printf("\nPress CTRL-C to exit.\n");
  printf("Type messages and click enter to send.\n");
  printf("-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --\n\n");
  
  //timeout.tv_sec = TIMEOUT;
  //timeout.tv_usec = 0;
  //FD_ZERO(&writefds);
  //FD_SET(connfd, &writefds);
  while(QUIT) {
    // Read input and send messages
    // TODO:
    fgets(buff, BUFF_SIZE, stdin);
    //Check if pipe was closed.
    //int p;
    //if( (p = select(connfd + 1, NULL, &writefds, NULL, &timeout)) <= 0 ) {
    //  break;
    //}
    write(connfd, buff, strlen(buff));
  }

  printf("\n-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --\n\n");
  printf("Connection with %s closed.\n", argv[1]);
  close(connfd);

  return 0;
}
