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
#include <fcntl.h>
#include "structures.h"

#define BUFF_SIZE 128 // TODO: Check what happens with a bigger input
#define TIMEOUT 120
#define MSG_STACK_SIZE 10
#define GLOBAL_LOG_FILENAME "server_global.log"

#define handle_err(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

// Clean up before exiting.
void clean (int);
// Add message to shared message stack.
void write_to_message_stack (char *, int);
// Receiver thread.
static void *receiver (void *);
// Writer thread.
static void *writer (void *);

// Switch that keeps the server listening for new connections.
static int quit = 0;
// List of active connections.
static struct ConnList *connections;
// Shared stack of messages.
static char msg_stack[MSG_STACK_SIZE][BUFF_SIZE];

pthread_mutex_t stack_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t connections_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t new_msg_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t stack_cond = PTHREAD_COND_INITIALIZER;
static int stack_availability = MSG_STACK_SIZE;
static int stack_index = 0;
static int new_msg = 0;
static int write_index = 0;
static int read_index = 0;


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
  struct sigaction action, old_action, ign_action;
  mode_t mode;
  int global_log_fd;

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
  if (pthread_create(&thread_id, NULL, &writer, &global_log_fd) != 0) {
    handle_err("Error on thread creation.");
  }

  // Clean when SIGINT is caught.
  action.sa_handler = &clean;
  action.sa_flags = SA_RESETHAND; //reset to default after use
  sigaction(SIGINT, &action, NULL);
  // Ignore SIGPIPE.
  ign_action.sa_handler = SIG_IGN;
  sigaction(SIGPIPE, &ign_action, NULL);
  
  // Listen for connections.
  printf("\nPress CTRL-C to exit.\n");
  printf("Listening for incoming connections.\n");
  printf("-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --\n\n");
  quit = 0;
  while(!quit) {
    client_addr_size = sizeof(struct sockaddr_in);
    connfd = accept(listenfd, (struct sockaddr*) &client_addr, &client_addr_size);
    if (connfd == -1) {
      clean(0); // SIGINT was received
      if (errno != EINTR) { // or other error
        handle_err("Error on accepting connection."); 
      }
    } else { // Connection received.
      // Create and append connection to connection list.
      if (! (connection = createConn(connfd, NULL, NULL))) {
        clean(0);
	handle_err("Error creating new connection node!"); 
      }
      appendConn(connection, connections);
      // Print new connection.
      inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
      printf("%s connected.\n", client_ip);
      // Create thread and pass connection.
      if (pthread_create(&thread_id, NULL, &receiver, connection) != 0) {
        clean(0);
        handle_err("Error on thread creation.");
      }
    }
  }
  
  close(global_log_fd);
  printf("\nDone.\n");


  return 0;
}

// Change QUIT value, to quit the main running loop,
// and clean up dynamically allocated memory.
void clean(int signo) {
  struct Node *node, *temp;

  if (quit) { return; } // clean() already run
  
  quit = 1;

  printf("\n-- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --\n\n");
  // Close open connections and 
  // free dynamically allocated memory.
  printf("Closing connections:\n");
  if ( (pthread_mutex_lock(&connections_mutex)) != 0) { // Mutex read/write
    handle_err("Error locking mutex.");
  }
  node = connections->head;
  while(node) {
    printf("Close %d.\n", node->value);
    close(node->value);
    temp = node;
    node = node->next;
    free(temp);
  }
  free(connections);
  if ( (pthread_mutex_unlock(&connections_mutex)) != 0) {
    handle_err("Error unlocking mutex.");
  }
}

// Receiver thread.
// Handles incoming messages from a client.
static void *receiver(void *arg) {
  struct Node *connection = (struct Node *) arg;
  int connfd = connection->value;
  int n, welcome;

  char buff[BUFF_SIZE];
  const char welcome_msg[] = "Welcome in the chatroom. You are id #%d.\n\n";
  const char entered_notification[] = "#%d Entered the chat.";
  const char left_notification[] = "#%d Left the chat.";
  
  // Send welcome message.
  sprintf(buff, welcome_msg, connfd);
  n = write(connfd, buff, strlen(buff));
  welcome = (n == -1 && errno == EPIPE) ? 0 : 1; // welcome = if welcome msg was received
  // Notify new connection to the chatroom.
  if (welcome) {
    sprintf(buff, entered_notification, connfd);
    write_to_message_stack(buff, strlen(buff));
  }

  while(welcome && !quit) {
    n = read(connfd, buff, BUFF_SIZE - 1);
    if (!n) { break; } // Check if pipe was closed!

    write_to_message_stack(buff, n);
 }

  // When client disconnects remove fd from list
  // and return.
  printf("Connection #%d interrupted.\n", connfd);
  if ( (pthread_mutex_lock(&connections_mutex)) != 0) { // Mutex write
    handle_err("Error locking mutex.");
  }
  removeConn(connection, connections);
  if ( (pthread_mutex_unlock(&connections_mutex)) != 0) {
    handle_err("Error unlocking mutex.");
  }

  // Notify connection termination to the chatroom.
  sprintf(buff, left_notification, connfd);
  write_to_message_stack(buff, strlen(buff));
}

// Writer thread.
// Writes to disk and send to every client the received messages.
static void *writer (void *arg) {
  char buff[BUFF_SIZE];
  int len;
  struct Node *node;

  // Open global log file.
  mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  int global_log_fd = open(GLOBAL_LOG_FILENAME, O_RDWR | O_CREAT | O_APPEND, mode);

  while(1) {
    if (pthread_mutex_lock(&stack_mutex) != 0) { // Mutex read
      handle_err("Error locking message stack mutex.\n");
    }
    while(!new_msg) { // loop for signals interrupting the wait
      pthread_cond_wait(&new_msg_cond, &stack_mutex);
    }
    // Read one message from the shared stack.
    snprintf(buff, BUFF_SIZE, msg_stack[read_index++]);
    if (read_index == MSG_STACK_SIZE) { read_index = 0; }// Reset to use stack round-robin
    new_msg--;
    stack_availability++;
    pthread_cond_signal(&stack_cond);
    if ( (pthread_mutex_unlock(&stack_mutex)) != 0) {
      handle_err("Error unlocking message stack mutex.");
    }

    // Format message.
    len = strlen(buff);
    buff[len] = '\n';
    buff[len + 1] = '\0';
    len++;

    // Send message to every client.
    if ( (pthread_mutex_lock(&connections_mutex)) != 0) { // Mutex read/write
      handle_err("Error locking connections mutex.");
    }
    for(node = connections->head; node; node = node->next) {
      // Send and check if pipe is broken
      if( (write(node->value, buff, len)) == -1 && errno == EPIPE) {
        removeConn(node, connections);
      }	
    }
    if ( (pthread_mutex_unlock(&connections_mutex)) != 0) {
      handle_err("Error unlocking connections mutex.");
    }

    // Write on disk.
    if ( (write(global_log_fd, buff, len)) <= 0) {
      handle_err("Error writing on global log.");
    }
  }

  close(global_log_fd);
}

// Add a message to the shared message stack.
void write_to_message_stack(char *message, int len) {
    if (pthread_mutex_lock(&stack_mutex) != 0) { // Mutex write
      handle_err("Error locking message stack mutex.\n");
    }
    while(!stack_availability) { // loop for signals interrupting the wait
      pthread_cond_wait(&stack_cond, &stack_mutex);
    }
    snprintf(msg_stack[write_index++], len, message); // Write in shared stack.
    if (write_index == MSG_STACK_SIZE) { write_index = 0; }// Reset to use stack round-robin
    new_msg++;
    stack_availability--;
    pthread_cond_signal(&new_msg_cond);
    if ( (pthread_mutex_unlock(&stack_mutex)) != 0) {  
      handle_err("Error unlocking message stack mutex.");
    }
}

// TODO: set up timer thread: Send date notifications to the chatroom.
//       when midnight strikes, send the date of the new day. For logs.
