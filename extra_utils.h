/***
*  extra_utils.h
*  Header with extra structures
*  Written by Filipe <mail@filipe.pt>
**/


#ifndef EXTRAUTILS_H
#define EXTRAUTILS_H

#define NCONNECTIONS       2
#define MAXHOSTNAME     2560
#define MAX_REQ          100

//structure of sockets
typedef struct {
  int sock;    // Socket ID
  int used;    // Whether this structure is occupied
  int inuse;   // Whether this socket is in use (pending request)
  // Other stuff for synchronization
  pthread_t t_conn;    /* Thread to monitor the socket (used for timeout) */
} socket_t;


//structure of servers
typedef struct {
  char host[MAXHOSTNAME];             // Origin server's domain name
  int portno;                         // Origin server's port number
  socket_t connection[NCONNECTIONS];  // Open connections to this server

  // Other stuff for synchronization
  pthread_mutex_t  mtx;   /* the lock the socket array */
  pthread_cond_t cv;   /* the element exists*/
} server_t;

server_t table[MAX_REQ];

/**
 * Shared functions 
**/

/* create a new fifo queue */
server_t *createNewTable();
 /* add new elements to the queue, adds to the tail */
int addElement(server_t element);
 /* get the elements from the table, keeps on the table*/
int getElement(server_t *element, char host[MAXHOSTNAME]);
/* update connection 2 from an element in the table */
int updateConnection(int index, int sock, int conn);
/* update unused element from the table */
int updateElement(int index, int conn);
 /* remove unused element from the table */
int removeElement(char *host, int conn);
 /* check if the table is full */
int isEmpty();
void printTable();

#endif
