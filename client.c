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
#include <time.h>
#include <ctype.h>
#include <pthread.h>

#define BUFF_SIZE 256
#define ID_SIZE 6
#define TIMEOUT 1
#define GLOBAL_LOG_FILENAME "client_global.log"
#define PERSONAL_LOG_FILENAME "client_personal.log"
#define handle_err(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

static int quit = 0;
static int id = 0;

static void *receiver (void *);

int format_message (char *, char *);

void clean (int signo) {
  if (quit) { return; } // clean() already run

  quit = 1;
}

int main (int argc, char **argv) {
  int connfd, portno, global_log_fd, len;
  struct sockaddr_in serv_addr;
  char buff[BUFF_SIZE], send_buff[BUFF_SIZE];
  struct sigaction action, ign_action;
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
  // Ignore SIGPIPE.
  action.sa_handler = &clean;
  action.sa_flags = SA_RESETHAND; // reset to default after use
  sigaction(SIGINT, &action, NULL);
  ign_action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &ign_action, NULL);

  printf("Connected to %s.\n", argv[1]);
  printf("\nPress CTRL-C to exit.\n");
  printf("Type messages and click enter to send.\n");
  printf("-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --\n\n");
  
  // Read input and send messages. Also check pipe.
  while(!quit) {
    if (! (fgets(buff, BUFF_SIZE, stdin))) { break; }
    len = format_message(buff, send_buff);
    if ( (write(connfd, send_buff, len)) == -1 && errno == EPIPE) { break; }
  }

  printf("\n-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --\n\n");
  printf("Connection with %s closed.\n", argv[1]);
  close(connfd);

  return 0;
}

// Format user input into chatroom message.
// Eg. add timestamp, user id, etc.
int format_message(char *message, char *formatted) {
  time_t seconds;
  struct tm bd_tm;
  char *format = "[#%d|%d:%d] %s"; // [#id|hour:minute] messaggio
 
  seconds = time(NULL);
  localtime_r(&seconds, &bd_tm);

  snprintf(formatted, BUFF_SIZE, "[#%d|%d:%d] %s", id, bd_tm.tm_hour, bd_tm.tm_min, message);

  return strlen(formatted);
}

// Receiver/Writer thread.
// Receive messages and write them to disk.
static void *receiver (void *arg) {
  char buff[BUFF_SIZE];
  int connfd = *((int *) arg);
  int n, i, received_id;
  char identifier[ID_SIZE], *p;

  // Open log files.
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  int global_log_fd = open(GLOBAL_LOG_FILENAME, O_RDWR | O_CREAT | O_APPEND, mode);  
  int personal_log_fd = open(PERSONAL_LOG_FILENAME, O_RDWR | O_CREAT | O_APPEND, mode);  


  while(1) {
    n = read(connfd, buff, BUFF_SIZE - 1);
    if (!n) { break; } // Check if pipe was closed!
    buff[n] = '\0';

    // Write on global log.
    if ( (write(global_log_fd, buff, n)) <= 0) {
      handle_err("Error writing on global log.");
    }

    // Message parsing. 
    // Get id. Do print messages sent by me.
    for(p = buff; *p != '#'; p++) { 
      ; 
    }
    p++;
    for (i = 0; isdigit(p[i]); i++) {
      identifier[i] = p[i];
    }
    identifier[i] = '\0';
    received_id = atoi(identifier);
    if (!id) {
      id = received_id;
      printf("%s", buff); // Print welcome message
    }

    // Print on screen.
    if (received_id != id) { // Weclcome message is not printed here
      printf("%s", buff);
    } else { // Write on personal log.
      if ( (write(personal_log_fd, buff, n)) <= 0) {
        handle_err("Error writing on global log.");
      }
    }

 }

  close(global_log_fd);
  close(personal_log_fd);
}

