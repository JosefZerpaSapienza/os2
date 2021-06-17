// client.c
//
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
#include <fcntl.h>

#define BUFF_SIZE 256
#define TIMEOUT 1
#define GLOBAL_LOG_FILENAME "client_global.log"
#define handle_err(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

int quit = 0;

static void *receiver (void *);

void clean (int signo) {
  if (quit) { return; } // clean() already run

  quit = 1;
}

int main (int argc, char **argv) {
  int connfd, portno, n;
  struct sockaddr_in serv_addr;
  char buff[BUFF_SIZE];
  struct sigaction action, ign_action;
  mode_t mode;
  int global_log_fd;
  pthread_t thread_id;

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

  // Spawn receiver thread.
  if (pthread_create(&thread_id, NULL, &receiver, &connfd) != 0) {
    handle_err("Error on thread creation.");
  }
  
  // Sender thread.
  // Exit when SIGINT is caught.
  action.sa_handler = &clean;
  action.sa_flags = SA_RESETHAND; // reset to default after use
  sigaction(SIGINT, &action, NULL);
  ign_action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &ign_action, NULL);

  printf("Connected to %s.\n", argv[1]);
  printf("\nPress CTRL-C to exit.\n");
  printf("Type messages and click enter to send.\n");
  printf("-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --\n\n");
  
  while(!quit) {
    // Read input and send messages. Also check pipe.
    fgets(buff, BUFF_SIZE, stdin);
    if ( (write(connfd, buff, strlen(buff))) == -1 && errno == EPIPE) { break; }
  }

  printf("\n-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --\n\n");
  printf("Connection with %s closed.\n", argv[1]);
  close(connfd);

  return 0;
}


// Receiver/Writer thread.
// Receive messages and write them to disk.
static void *receiver (void *arg) {
  char buff[BUFF_SIZE];
  int connfd = *((int *) arg);
  int n;

  // Open global log file.
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  int global_log_fd = open(GLOBAL_LOG_FILENAME, O_RDWR | O_CREAT | O_APPEND, mode);  

  while(1) {
    n = read(connfd, buff, BUFF_SIZE - 1);
    if (!n) { break; } // Check if pipe was closed!

    buff[n] = '\0';
    // Print on screen.
    printf("%s", buff);

    // Write on disk.
    if ( (write(global_log_fd, buff, n)) <= 0) {
      handle_err("Error writing on global log.");
    }
  }

  close(global_log_fd);
}

