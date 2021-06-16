// server.c
//
// Server of a chat room.
// author: Josef E. Zerpa R.

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include "structures.h"

#define BUFF_SIZE 256
#define TIMEOUT 120
#define handle_err(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

// Clean up before exiting.
void quit (int signo);
// Receiver thread.
static void *receiver (void *arg);

// Switch that keeps the server listening for new connections.
static int QUIT = 0;
// List of active connections.
static struct ConnList *connections;

// Chatroom server. 
// Listen for connections and manage thread creation
// for every new client.
int main (int argc, char **argv) {
  int port = 0, connection_limit = 10;
  int listenfd, connfd;
  char client_ip[INET_ADDRSTRLEN];
  struct sockaddr_in serv_addr, client_addr;
  int client_addr_size;
  pthread_t thread_id;
  struct Node *connection;
  struct sigaction action, old_action;

  // Check options
  switch (argc) 
  {
    case 3:
      connection_limit = atoi(argv[2]);
      if (connection_limit == 0) {
        handle_err("Error parsing connection limit provided.");
      }
    case 2:
      port = atoi(argv[1]); 
      if (port == 0) { 
        handle_err("Error parsing port provided."); 
      }
      break;
    default:
      printf("USAGE: %s PORT_NUMBER [CONNECTION_LIMIT]\n"\
      "Default connection limit is 10.\n", argv[0]);
      handle_err("No port number provided.");
      break;
  }

  printf("Setting up server with\n");
  printf("Port: %d\n", port);
  printf("Connection limit: %d\n", connection_limit);

  // Set up socket.
  if ( (listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    handle_err("Error opening socket");
  }
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(port);
  if ( bind(listenfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1) {
    handle_err("Error on binding.");
  }
  if ( listen(listenfd, connection_limit) == -1 ) {
    handle_err("Error on linstening.");
  }

  // Set up fd list.
  connections = (struct ConnList *) malloc(sizeof(struct ConnList));
  connections->head = connections->tail = NULL;
  
  // Set up Writer thread.
  // TODO: writer thread.

  // Clean when SIGINT is caught.
  action.sa_handler = &quit;
  action.sa_flags = SA_RESETHAND; //reset to default after use
  sigaction(SIGINT, &action, NULL);
  
  // Listen for connections.
  printf("\nPress CTRL-C to exit.\n");
  printf("Listening for incoming connections.\n");
  printf("-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --\n\n");
  QUIT = 0;
  while(!QUIT) {
    client_addr_size = sizeof(struct sockaddr_in);
    connfd = accept(listenfd, (struct sockaddr*) &client_addr, &client_addr_size);
    if (connfd == -1) {
      quit(0); // SIGINT was received
      if (errno != EINTR) { // or other error
        handle_err("Error on accepting connection."); 
      }
    } else {
      // Connection received.
      // Create and append connection to connection list.
      if (! (connection = createConn(connfd, NULL, NULL))) {
        quit(0);
	handle_err("Error creating new connection node!"); 
      }
      appendConn(connection, connections);
      // Print new connection.
      inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
      printf("%s connected.\n", client_ip);
      // Create thread and pass connection.
      if (pthread_create(&thread_id, NULL, &receiver, connection) != 0) {
        quit(0);
        handle_err("Error on creating thread.");
      }
    }
  }
  
  printf("\nDone.\n");


  return 0;
}

// Change QUIT value, to quit the main running loop.
void quit(int signo) {
  struct Node *node, *temp;

  if (QUIT) { return; } // quit() already run
  
  QUIT = 1;

  printf("\n-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --\n\n");
  // Close open connections and 
  // free dynamically allocated memory.
  printf("Closing connections:\n");
  node = connections->head;
  while(node) {
    printf("Close %d.\n", node->value);
    // TODO: when connection is closed exit client program.
    // shutdown(node->value, SHUT_RD);
    close(node->value);
    temp = node;
    node = node->next;
    free(temp);
  }
  free(connections);
}

// Receiver thread.
// Handles the connection with a client and
// receives incoming messages.
static void *receiver(void *arg) {
  struct Node *connection = (struct Node *) arg;
  int connfd = connection->value;
  int retval, n;

  char buff[BUFF_SIZE];
  const char welcome_msg[] = "Welcome in the chatroom. You are fd #%d.\n";
  struct timeval timeout;
  fd_set readfds;
  
  // Send welcome message.
  sprintf(buff, welcome_msg, connfd);
  write(connfd, buff, strlen(buff));

  // Check if pipe was closed!
  timeout.tv_sec = TIMEOUT;
  timeout.tv_usec = 0;
  FD_ZERO(&readfds);
  FD_SET(connfd, &readfds);
  while(select(connfd + 1, &readfds, NULL, NULL, &timeout) > 0) {
    // TODO: Read message and forward to writer thread.
    n = read(connfd, buff, BUFF_SIZE - 1);
    if (n <= 0) { break; }
    buff[n - 1] = '\0';
    puts(buff);
  }

  // When client disconnects remove fd from list
  // and return.
  printf("Connection #%d interrupted.\n", connfd);
  removeConn(connection, connections);
}


