// structures.c
// File containing code for the structures used.
#include <stdlib.h>


// Linked list.

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
  if (!node) { return 0; }

  node->value = value;
  node->next = next;
  node->previous = previous;

  return node;
}


// ConnList (ConnectionList).

// ConnList is a list of connections.
// ConnList is actually just a linked list. Each node represents 
// a connection. It's 'value' field holds the fd to the connection.
struct ConnList {
  // Head of linked list of free indices.
  struct Node *head;
  // Tail of linked list of free indices.
  struct Node *tail;
};

// Allocate a new connection node, with the given values set.
struct Node *createConn(int fd, struct Node *next, struct Node *previous) {
  struct Node *node = getNode(fd, next, previous);
  if (!node) { return 0; }

  return node;
}
// Append the given Node to the given ConnList.
// The node address is returned.
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

// Remove the node at the address given, from the ConnList.
// @WARNING! IT'S NOT CHECKED wether the given node actually
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
