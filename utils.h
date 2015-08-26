/***
*  Utility functions header
*  utils.h
**/

#ifndef NETWORK_H
#define NETWORK_H

extern int passivesocket();    /* (int portnumber) */
  /* Used by the server to open a passive socket attached to the port 
     numbered 'portnumber' on the server's machine, allowing a queue length 
     of 5 requests.  Returns the socket descriptor. */

extern int activesocket();     /* (char *hostname, int portnumber) */
  /* Used by the client to connect to the port numbered 'portnumber' on the 
     server's machine, whose domain name is 'hostname', opening an active 
     socket for the connection.  Returns the socket descriptor.  */

extern int acceptconnection(); /* (int socket) */
  /* Used by the server to accept a connection request from a client on a 
     passive socket, 'socket'.  When a request arrives, this function 
     allocates a new socket for the connection, and returns its descriptor.  */

extern int acceptfrom(); /* (int socket, char *client, int size) */
  /* Same as 'acceptconnection' except that the IP address of the connecting
     client's machine is placed in the 'client' string.  No more than 'size'
     characters are copied into 'client'.  */

extern int TCPreadline();      /* (int socket, char *buffer, int size) */
  /* Reads from a TCP socket 'socket' into a buffer 'buffer' of size 'size'.  
     All characters up to and including the first end of line ('\n') character 
     will be read, up to a maximum of size-1 bytes.  The string read will 
     be terminated with a null character.  The function returns the length 
     of the string read.  */

extern int HTTPreadheader();    /* (int socket, char *buffer, int size) */
  /* Reads an HTTP message header from a TCP socket into a buffer 'buffer'
     of size 'size'.  All characters up to and including the first
     "\r\n\r\n" sequence will be read, up to a maximum of size-1 bytes.
     The string read will be terminated with a null character.  The
     function returns the length of the string read.  */

extern int HTTPheadervalue();   /* (char *header, char *key, char *value) */
  /* Searches an HTTP message header (read by 'HTTPreadheader') for a line
     with field name 'key'.  If found, the rest of the matching line (after
     the ": ") is returned in 'value' as a null-terminated string, and the
     function returns 1; otherwise the function returns 0.  */

extern void HTTPheaderremove();   /* (char *header, char *key) */
  /* Searches an HTTP message header (read by 'HTTPreadheader') for a line
     with field name 'key'.  If found, the whole line is deleted from the 
     header.  */

extern int HTTPheadervalue_case();   /* (char *header, char *key, char *value) */
  /* Searches an HTTP message header (read by 'HTTPreadheader') for a line
     with field name 'key'.  If found, the rest of the matching line (after
     the ": ") is returned in 'value' as a null-terminated string, and the
     function returns 1; otherwise the function returns 0.  */

extern void HTTPheaderremove_case();   /* (char *header, char *key) */
  /* Searches an HTTP message header (read by 'HTTPreadheader') for a line
     with field name 'key'.  If found, the whole line is deleted from the 
     header.  */

#endif
