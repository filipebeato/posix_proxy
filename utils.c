#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>

int passivesocket(int portnumber)
{
    struct sockaddr_in addr;
    int x;

    x = socket(AF_INET, SOCK_STREAM, PF_UNSPEC);/* Open the socket */
    if(x < 0)                                   /* failed ? */
        return -1;
    memset(&addr, 0, sizeof(addr));             /* Clear the socket address */
    addr.sin_family = AF_INET;                  /* Internet addressing */
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(portnumber);          /* 10000 .. 20000 */
    if(bind(x, (struct sockaddr*) &addr, sizeof(addr)) < 0)
        return -1;
    if(listen(x, 5) < 0)                        /* Accept a queue of 5 */
        return -1;
    return x;
}

int activesocket(char *hostname, int portnumber)
{
    struct sockaddr_in addr;
    struct hostent *h;
    int x;

    h = gethostbyname(hostname);                /* What is the address */
    if(h == NULL)                               /* hostname not found??*/
    {
        fprintf(stderr, "%s: doesn't exist!\n", hostname);
        return -1;
    }
    x = socket(AF_INET, SOCK_STREAM, PF_UNSPEC);/* Open the socket */
    if(x < 0)                                   /* failed ? */
        return -1;
    memset(&addr, 0, sizeof(addr));             /* Clear the socket address */
    addr.sin_family = AF_INET;                  /* Internet addressing */
    memcpy(&addr.sin_addr.s_addr, h -> h_addr, sizeof(int)); /*address */
    addr.sin_port = htons(portnumber);                 /* 10000 .. 20000 */
    if(connect(x, (struct sockaddr*) &addr, sizeof(addr)) < 0)
        return -1;
    return x;
}

int acceptfrom(int socket, char *client, int size)
{
    struct sockaddr_in addr;
    struct linger l;
    int s, i;
    i = sizeof(addr);
    s = accept(socket, (struct sockaddr *)&addr, &i);
    if(s < 0)
        return -1;
    l.l_linger = 5;
    l.l_onoff = 1;
    setsockopt(s, SOL_SOCKET, SO_LINGER, (char *)&l, sizeof(l));
    strncpy(client, inet_ntoa(addr.sin_addr), size);
    client[size-1] = '\0';
    return s;
}

int acceptconnection(int socket)
{
    char client[40];
    return acceptfrom(socket, client, 40);
}

int TCPreadline(int s, char *buffer, int size)
{
    char *b;
    int n, count;

    b = buffer;
    count = 0;
    while ((n = read(s, b, 1)) > 0)
    {
        count ++;
        if (*b == '\n' || count == size-1)
            break;
        b ++;
    }
    if (n < 0) return n;
    buffer[count] = '\0';
    return count;
}

int HTTPreadheader(int s, char *buffer, int size)
{
    char *b;
    char last;
    int n, count;

    b = buffer;
    last = '\n';
    count = 0;
    while ((n = read(s, b, 1)) > 0)
    {
        count ++;
        if ((*b == '\n' && last == '\n') || count == size-1)
            break;
        if (*b != '\r') last = *b;
        b ++;
    }
    if (n < 0)
        return n;
    buffer[count] = '\0';
    return count;
}

int HTTPheadervalue(char *header, char *key, char *value)
{
    char *b = header;
    int i;

    while (1)
    {
        b = strstr(b, key);
        if (b == NULL) return 0;
        if (b == header || *(b-1) == '\r' || *(b-1) == '\n') break;
        b++;
    }
    b += strlen(key)+1;
    while (*b == ' ')
        b++;
    i = strcspn(b, "\n\r");
    strncpy(value, b, i);
    value[i] = '\0';
    return 1;
}

void HTTPheaderremove(char *header, char *key)
{
    char *b = header;
    char *nextline;

    while (1)
    {
        b = strstr(b, key);
        if (b == NULL) return;
        if (b == header || *(b-1) == '\r' || *(b-1) == '\n') break;
        b++;
    }
    nextline = b;
    while (*nextline != '\n')
        nextline++;
    nextline++;
    while (*nextline != '\0')
        *b++ = *nextline++;
    *b = '\0';
}

char *strstr_case(char *p, char *key)
{
    int len = strlen(key);

    while (*p != '\0')
        if (strncasecmp(p, key, len) == 0)
            return p;
        else
            p++;
    return NULL;
}

int HTTPheadervalue_case(char *header, char *key, char *value)
{
    char *b = header;
    int i;

    while (1)
    {
        b = strstr_case(b, key);
        if (b == NULL) return 0;
        if (b == header || *(b-1) == '\r' || *(b-1) == '\n') break;
        b++;
    }
    b += strlen(key)+1;
    while (*b == ' ')
        b++;
    i = strcspn(b, "\n\r");
    strncpy(value, b, i);
    value[i] = '\0';
    return 1;
}

void HTTPheaderremove_case(char *header, char *key)
{
    char *b = header;
    char *nextline;

    while (1)
    {
        b = strstr_case(b, key);
        if (b == NULL) return;
        if (b == header || *(b-1) == '\r' || *(b-1) == '\n') break;
        b++;
    }
    nextline = b;
    while (*nextline != '\n')
        nextline++;
    nextline++;
    while (*nextline != '\0')
        *b++ = *nextline++;
    *b = '\0';
}
