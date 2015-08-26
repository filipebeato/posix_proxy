/***
*  Multi Thread (POSIX) Transparent Proxy server
*  extra_utils.h
*  Header with extra structures
*  Written by Filipe <mail@filipe.pt>
**/

#include <limits.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <poll.h>
#include "utils.h"
#include "extra_utils.h"


// Define constants
#define MAX_HEADER     20480
#define MAX_LINE        2560
#define BSIZ            8192
#define GET                0
#define POST               1
#define MAX_NUMBER      1000

#define TRUE               1
#define FALSE              0
/* 10 seconds timeout (for the client) */
#define TIMEOUT        10000
/* 20 seconds timeout (for the server) */
#define TIMEOUT2       20000


char *methods[2] = {"GET", "POST"};

//structure containing arguments to pass into consumer and producer
typedef struct thread_arg {
   socket_t *s;
   char *host;
   pthread_mutex_t *m;
   int *conn;
} th_args;


int contentlength(char *header){
    int len=INT_MAX;
    char line[MAX_LINE];

    if (HTTPheadervalue_case(header, "Content-Length", line))
        sscanf(line, "%d", &len);
    return len;
}

void parserequest(char *request, int *method, char *host, int *pn, char *path){
    char url[MAX_LINE] = "";
    char m[MAX_LINE] = "";

    char hostport[MAX_LINE] = "";
    char port[MAX_LINE] = "";
    int i;

    sscanf(request, "%[^ ] %[^ ] HTTP/1.1", m, url);
    for (i=0; i<2; i++)
        if (strcmp(m, methods[i]) == 0)
            *method = i;

    sscanf(url, "http://%[^\n\r/]%[^\n\r]", hostport, path);
    sscanf(hostport, "%[^:]:%[^\n\r]", host, port);

    if (*port == '\0')
        *pn = 80;
    else
        *pn = atoi(port);
}

void copydata(int from, int to, int len){
    char tbuff[BSIZ];
    int n;
    while (len > 0){
        if ((n = read(from, tbuff, BSIZ)) <= 0) break;
        if (write(to, tbuff, n) < n) break; 
        len -= n;
    }
}

/**
 * Thread to monitor the timeout of the server connections
 **/
void *monitorsocket(void *arg){
    //buffers init
    th_args *targs;
    targs = (th_args *) arg;
    socket_t *sck = targs->s;
    struct pollfd fds[1];
    fds[0].fd = sck->sock; //monitor the socket if there is any "movement"
    fds[0].events = POLLIN | POLLPRI;

    while(1){
		//activates only when there is a change on the value inuse from  1 to 0
        while(sck->used && !sck->inuse);
        
		//checks the connection timeout
        if((poll(fds, 1, TIMEOUT2)) == 0) {
            printf("Server TimeOut!\n");
            pthread_mutex_lock(targs->m);
			//remove the element only if the other connection have inuse = 0 (is not being used)
            removeElement(targs->host, *targs->conn);
			//close the socket
            close(sck->sock);
            pthread_mutex_unlock(targs->m);
            break;
        }
    }
    //free();
}

/**
 * Builds a new element on the table for the host = hostname
 **/
void buildNewElement(server_t *new, char *hostname, int portnr, int sock){
        printf("build element: \n host: %s \n", hostname);
        strcpy(new->host, hostname);
        new->portno = portnr;
        new->connection[0].sock = sock;
        new->connection[0].used = TRUE;
        new->connection[0].inuse = TRUE;
        new->connection[1].sock = 0;
        new->connection[1].used = FALSE;
        new->connection[1].inuse = FALSE;
        pthread_cond_init(&new->cv, NULL);
        pthread_mutex_init(&new->mtx, NULL);


        //creates two threads to monitor the timeout 
        //creates structure with arguments to the threads to monitor each socket
        th_args *t_args1, *t_args2;

        t_args1 = (th_args*)malloc(sizeof(th_args));
        socket_t *sck1 = (socket_t*)malloc(sizeof(socket_t));
        int *argum1 = (int *)malloc(sizeof(int));
        *argum1 = 0;
        *sck1 = new->connection[0];
        t_args1->s = sck1;
        t_args1->m = &new->mtx;
        t_args1->conn = argum1;
        t_args1->host = strdup(hostname);
        t_args2 = (th_args*)malloc(sizeof(th_args));
        socket_t *sck2 = (socket_t*)malloc(sizeof(socket_t));
        int *argum2 = (int *)malloc(sizeof(int));
        *argum2 = 1;
        *sck2 = new->connection[1];
        t_args2->s = sck2;
        t_args2->m = &new->mtx;
        t_args2->conn = argum2;
        t_args2->host = strdup(hostname);

    	//initialize the threads to monitor each connection socket
        pthread_create(&new->connection[0].t_conn, NULL, monitorsocket, t_args1);
        pthread_detach(new->connection[0].t_conn);
        pthread_create(&new->connection[1].t_conn, NULL, monitorsocket, t_args2);
        pthread_detach(new->connection[1].t_conn);
}


/**
 * Service connection to client with connected to socket 'cli'
 * - Receives requests and processes the responses for one client, will close if client asks, timeout or invalid length
 * @param cli = client socket descriptor
 **/
void *serviceconn(void *client){
    //buffers init
    int cli = *(int*) client;
    char reqline[MAX_LINE], resline[MAX_LINE];
    char reqhead[MAX_HEADER], reqhead1[MAX_HEADER], reshead[MAX_HEADER];
    char host[MAX_LINE], path[MAX_LINE], servclose[MAX_LINE], servcontent[MAX_LINE], cliclose[MAX_LINE], contlength[MAX_LINE];

    //Timeout poll definitions
    struct pollfd fds[1];
    fds[0].fd = cli;
    fds[0].events = POLLIN | POLLPRI;

    int srv, v, method, portno, status, ret = TRUE, data, srvdata, closesrv=FALSE, localhostindex, presentconn, nospace=FALSE, resend=FALSE, accepted;

    //Flag ret. The client/server connection will be kept until ret=FALSE (used to still send the end response) or break
    while(ret){
        presentconn=0;
        accepted = TRUE;
        //Timeout: close connection whenever poll reaches TIMEOUT
        if((poll(fds, 1, TIMEOUT)) == 0) {
            printf("Client TimeOut!\n");
            break;
        }
        
        if(!resend){
            //Header request reading
            data = TCPreadline(cli, reqline, MAX_LINE); 
            parserequest(reqline, &method, host, &portno, path);
    
            //Asynchronous client connection close - if no data received and the header is empty
            if((HTTPreadheader(cli, reqhead, MAX_HEADER) <= 0)&&(data <= 0)){
                printf("Asynchronous close: Client Drop connection\n"); 
                break;
            }
    
            //printf("Reqline: %s Reqhead: %s \n end\n", reqline, reqhead);
            HTTPheadervalue_case(reqhead, "Connection", cliclose);
    
            //Control to close client connection, still process the request and response
            ret = (strcmp(cliclose, "close")==0) ? FALSE : TRUE;
            HTTPheaderremove_case(reqhead, "Connection");
        }

        //----------------------------------------------------------------------------------
        //Check if the host is present in the table
        server_t srv_t;
        printf("----------->Check element [%s]<-----------\n", host);
       
        if((localhostindex=getElement(&srv_t, host))!=-1){

            //lock the connection
            pthread_mutex_lock(&table[localhostindex].mtx);

            while(accepted){            
                //host already on the table
                printf("GET Element: ----------- start\n");
                strcpy(host, table[localhostindex].host);
                portno = table[localhostindex].portno;

                //check if there is any free connection
				//first slot was set to free due to a connection close
                if(!table[localhostindex].connection[0].used && !table[localhostindex].connection[0].inuse){//use=0 and inuse=0
                        if((srv = activesocket(host, portno)) < 0)
                            nospace = TRUE;
                        else{
                            updateConnection(localhostindex, srv, 0);
                            presentconn = 0;
                        }
                        accepted = FALSE;
                //second slot is still empty - add a new connection
                }else if(!table[localhostindex].connection[1].used && !table[localhostindex].connection[1].inuse){//use=0 and inuse=0
                        if((srv = activesocket(host, portno)) < 0)
                            nospace = TRUE;
                        else{
                            updateConnection(localhostindex, srv, 1);
                            presentconn = 1;
                        }
                        accepted = FALSE;
                //first slot is free to be used - use the existent socket of that connection
                }else if(table[localhostindex].connection[0].used && !table[localhostindex].connection[0].inuse){//use=1 and inuse=0
                        srv = table[localhostindex].connection[0].sock;
                        table[localhostindex].connection[0].inuse = TRUE;
                        presentconn = 0;
                        accepted = FALSE;
                //second slot is free to be used - use the existent socket of that connection
                }else if(table[localhostindex].connection[1].used && !table[localhostindex].connection[1].inuse){//use=1 and inuse=0
                        srv =table[localhostindex].connection[1].sock;
                        table[localhostindex].connection[1].inuse = TRUE;
                        presentconn = 1;
                        accepted = FALSE;
                }else{
                        //wait for an empty connection slot
                        if(table[localhostindex].connection[0].inuse && table[localhostindex].connection[1].inuse){
                            printf("----------->WAIT STATE<-----------\n");
                            pthread_cond_wait(&table[localhostindex].cv, &table[localhostindex].mtx);
                        }else{  
                              accepted = FALSE; 
                              nospace = TRUE;
                        }
                }
                printf("GET Element: ----------- end\n");
            }
            //unlock the connection
            pthread_mutex_unlock(&table[localhostindex].mtx);
        }
        else{   //Host is not present in any slot of the table
                //check if there is any available space in the table if there is not drop the packet
                printf("ADD Element: ----------- start\n");
                
                if((srv = activesocket(host, portno)) < 0){ 
                    nospace = TRUE;
                }
                else{
                    buildNewElement(&srv_t, host, portno, srv);

                    if(addElement(srv_t)){
                        localhostindex = 0;
                        printf("Element [%s] was added\n", srv_t.host);
                        presentconn = 0;
                    }
                    else {//drop the connection - there are no more slots available on the table to be used
                        printf("----------->Drop connection\n");
                        nospace = TRUE;
                    }
                    
                }
                accepted = FALSE;
                printf("ADD Element: ----------- end\n");
        }

        printf("----------->End check element [%s]<-----------\n", host);

        //Deal with socket description - Request to the server
        if(nospace){
            printf("----------->Fail to connect to the server [%s]<-----------\n", nospace,host);
            sprintf(reshead, "HTTP/1.1 503\r\nContent-Length: 12\r\nConnection: close\r\n\r\nNon-existent");
            //reply to the client (No active socket, Fail to connect to the server)
            write(cli, reshead, strlen(reshead));
            ret = FALSE;
        }

        //Server request - Client Response
        else{
            printf("----------->Process Request [%s]<-----------\n", host);
            sprintf(reqhead1, "%s %s HTTP/1.1\r\n", methods[method], path);
            strcat(reqhead1, "Connection: close\r\n");
            strcat(reqhead1, reqhead);
            //write to the server socket
            write(srv, reqhead1, strlen(reqhead1));
           // printf("%s", reqhead1);

            if (method == POST){
                //if there is no contenth length in a request with body (POST) send the response and close connection
                if((HTTPheadervalue_case(reqhead, "Content-Length", contlength))==0){
                    printf("Invalid Content Length\n");
                    closesrv = TRUE;
                    ret = FALSE;
                }
                copydata(cli, srv, contentlength(reqhead));
            }

            //read response from the server
            srvdata = TCPreadline(srv, resline, MAX_LINE);
            sscanf(resline, "HTTP/1.%d %d ", &v, &status);
            
            //Asynchronous client connection close - if no data received and the header is empty
            if((HTTPreadheader(srv, reshead, MAX_HEADER) == 0)&&(srvdata == 0)){
                printf("Asynchronous close: Server Drop connection [%s]\n", host);
                removeElement(host, localhostindex);
				//resend - controls the resend from the asynchronous connection, allowing only two tries
                if((method == GET)&&(resend < 2)) {resend++; continue;}
                else{ close(srv); break;}
            }

            HTTPheaderremove_case(reshead, "Connection");
            write(cli, resline, strlen(resline));
            printf("%s", resline);

            //check if there is connection close to close the server
            HTTPheadervalue_case(reshead, "Connection", servclose);
            closesrv = (strcmp(servclose, "close")==0) ? FALSE : TRUE;

            if(HTTPheadervalue_case(reshead, "Content-length", servcontent) == 0){
                printf("Invalid content length\n");
                closesrv = TRUE;
            }

            //write connection close if connection: close or invalid content length
            if(!ret){
                strcpy(resline, "Connection: close\r\n");
                write(cli, resline, strlen(resline));
            }

            //write to the client
            write(cli, reshead, strlen(reshead));
            printf("%s", reshead);

            //Copy from the server to the client the content
            if (status != 204 && status != 304)
                copydata(srv, cli, contentlength(reshead));

            //set inuse = false & used = true
            updateElement(localhostindex, presentconn);
            
        }
        printf("----------->End Process Request [%s]<-----------\n",host);
        
        //close server if the response has connection close or content length is invalid
        if(closesrv){ removeElement(host, localhostindex); close(srv);}
    }
    //if the client requests or timeout
    printf("Client got return value %d (Connection Closed)\n", ret);
    free(client);

    //close the client socket
    close(cli);
}

//---------------------------------------------------------------------------------------------------
// Create simple serviceconn
//---------------------------------------------------------------------------------------------------
void createSimpleThread(int arg){
  pthread_t t_prod;
  int *argum = (int *)malloc(sizeof(int));
  *argum = arg;
   
   //create the to the producer 
  pthread_create(&t_prod, NULL, serviceconn, argum);
  pthread_detach(t_prod);
}
//---------------------------------------------------------------------------------------------------

int main(int argc, char *argv[]){
   int request, s;
   createNewTable();
   if(argc > 1){
        s = passivesocket(atoi(argv[1]));
 
        //Protect to no busy port assigment
        if(s != -1){
            printf("..........Starting Proxy..........\n");   
            signal(SIGPIPE, SIG_IGN);

            for(;;){
                request = acceptconnection(s);
                printf("New connection (Count %d)\n", request);
                createSimpleThread(request);
            }
        }else{
            printf("Port %s busy. Choose a different or wait for a few seconds and try again...\n", argv[1]);
            exit(0);
        }
  }else
        printf("Add arguments:\n Run simple proxy: proxy [-s] [port number]\n Run proxy with pipeline: proxy [port number]\n");
}
