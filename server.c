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

#define BUFF_SIZE 256
#define TIMEOUT 120
#define handle_err(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

// Node of a linked list. Nodes are both forward and backwards linked.
struct Node {
  int value;
  struct Node *next;
  struct Node *previous;
};

// Returns a pointer to a Node structure.
// The Node is initialized with the values of 
// value and next provided.
struct Node *getNode (int value, struct Node *next, struct Node *previous) {
  struct Node *node = malloc(sizeof(struct Node));
  if (!node) { handle_err("malloc error"); }

  node->value = value;
  node->next = next;
  node->previous = previous;

  return node;
}

// FdList structure contains and array of file descriptors.
// A linked list is mainteined, composed by nodes whom values
// are the indices in the fd array that do not hold active fds.
struct ConnList {
  // Head of linked list of free indices.
  struct Node *head;
  // Tail of linked list of free indices.
  struct Node *tail;
};

struct Node *createConn(int fd, struct Node *next, struct Node *previous) {
  struct Node *node = getNode(fd, next, previous);
  if (!node) { handle_err("malloc error"); }

  return node;
}

// Allocate a new Node, and add the connection to the
// ConnList, with fd value. The node address is returned.
void  *appendConn(struct Node *node, struct ConnList *list) {
  node->previous = list->tail;
  node->next = NULL;
	  
  if (list->tail) { // tail exists
    list->tail->next = node;
  }
  list->tail = node; // update tail

  if (!list->head) { // head is not set
    list->head = node; // set head
  } 

}

// Removes the node at the address given, from the ConnList.
// @WARNING! IT's not checked wether the given node actually
// belongs to the given list.
void removeConn(struct Node *node, struct ConnList *list) {
  struct Node *previous = node->previous;
  struct Node *next = node->next;

  if (previous) { 
    previous->next = next; 
  } else { // this is head
    list->head = next;
  }
  if (next) { 
    next->previous = previous; 
  } else { // this is tail
    list->tail = previous;
  }

}


//List of open file descriptors (active connections).
static struct ConnList *connections;

// Clean the environment.
// Close open connections and free memory.
void clean(int signo) {
  struct Node *node, *temp;

  printf("\nClosing connections.\n");
  // Close open connections and 
  // free dynamically allocated memory.
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
  if (connections) {
    free(connections);
  }

}

// Receiver thread.
// Handles the connection with a client and
// receives incoming messages.
static void *receiver(void *arg) {
  struct Node *connection = (struct Node *) arg;
  int connfd = connection->value;
  int retval, n;

  char buff[BUFF_SIZE];
  const char welcome_msg[] = "Connected. You are fd #%d.\n";
  
  sprintf(buff, welcome_msg, connfd);
  write(connfd, buff, strlen(buff));

  // Check if pipe was closed!
  struct timeval timeout;
  timeout.tv_sec = TIMEOUT;
  timeout.tv_usec = 0;
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(connfd, &readfds);
  while(select(connfd + 1, &readfds, NULL, NULL, &timeout) > 0) {
    // TODO: Read message and send to writer thread.
    n = read(connfd, buff, BUFF_SIZE - 1);
    if (n <= 0) { break; }
    buff[n] = '\0';
    puts(buff);
  }

  // When client disconnects remove fd from list
  // and return.
  printf("Connection #%d interrupted.\n", connfd);
  removeConn(connection, connections);
}


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

  printf("Setting up server on port %d. Connection limit set to: %d.\n\n", port, connection_limit);

  // Set up fd list.
  connections = (struct ConnList *) malloc(sizeof(struct ConnList));
  connections->head = connections->tail = NULL;

  // Set up Writer thread.


  // Set up socket.
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if (listenfd == -1) {
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
  
  // Clean when SIGINT is caught.
  action.sa_handler = &clean;
  action.sa_flags = SA_RESETHAND; //reset to default after use
  sigaction(SIGINT, &action, NULL);
  // Listen for connections.
  printf("Listening for incoming connections.\n");
  printf("Press CTRL-C to exit.\n\n");
  while(1) {
    client_addr_size = sizeof(struct sockaddr_in);
    connfd = accept(listenfd, (struct sockaddr*) &client_addr, &client_addr_size);
    if (connfd == -1) {
      if (errno == EINTR) { // SIGINT was received
	printf("Exiting...\n");
	break;
      }
      handle_err("Error on accepting connection.");  
    } else {
      // Connection received.
      // Create and append connection to connection list.
      connection = createConn(connfd, NULL, NULL);
      appendConn(connection, connections);
      // Print new connection.
      inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
      printf("%s connected.\n", client_ip);
      // Create thread and pass connection.
      if (pthread_create(&thread_id, NULL, &receiver, connection) != 0) {
        clean(0);
        handle_err("Error on creating thread.");
      }
    }
  }

  close(listenfd);

  return 0;
}
