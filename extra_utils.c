/***
*  extra_utils.c
*  Thread-Safe FIFO queue definition and function to handle 
*  requests from the consumer.
*  Written by Filipe <mail@filipe.pt>
**/


#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "extra_utils.h"


// mutex that locks the access of the queue
pthread_mutex_t m_table;
//present size/ occupied space
int table_size;


/* Mutex lock and unlock functions*/
void lockmtx(){
        pthread_mutex_lock(&m_table);
}

void unlockmtx(){
        pthread_mutex_unlock(&m_table);
}

void condwait(int index){
        pthread_cond_wait(&table[index].cv, &m_table);
}


int isEmpty(){
        if(table_size >= 0)
                 return 0;
        return 1;
}

int isFull(){
        printf("It is Full, no more connections available %d\n",table_size);
        if(table_size == (MAX_REQ-1))
                 return 1;
        return 0;
}

void printTable(){
      int i;
      printf("Table Size: %d --------------------\n", table_size);
      for(i=0;i<=table_size;i++){
          printf("[%d]: %s, %d\n", i, table[i].host, table[i].portno);
      }
      printf("------------------------------\n");
}


/* External functions (using mutex to keep the queue thread safe)*/

/**
 * Create a new table
 * No need to lock because it will be the first time that it will be used
 * @return new fifo queue
**/
server_t *createNewTable(){
    //init the general mutex
    pthread_mutex_init(&m_table, NULL);
    table_size = -1;
    return table;
}

int isHostPresent(char host[MAXHOSTNAME]){
    int i, host_index=-1;
    for(i =0; i < MAX_REQ; i++){
        if(strcmp(table[i].host, host)==0){
                host_index = i;
                break;
        }
    }  
    return host_index;
}
/**
 * Add new element (thread safe) - locks the queue using mutexes and cond variables
 * @param fifo queue, the 2elements 
 * @return void
**/
int addElement(server_t element){
    //lock the table
    pthread_mutex_lock(&m_table);
    int ret = (isFull()==0) ? 1 : 0;
    if(ret){
        //add element to table
        table[table_size] = element;
        table_size ++;  
    }
    //unlock the table
    pthread_mutex_unlock(&m_table);
    return ret;
}

/**
 * Get/Remove element (thread safe) - locks the queue using mutexes and cond variables
 * @param fifo queue 
 * @return element from the head of the queue
**/
int getElement(server_t *element, char host[MAXHOSTNAME]){
    // printf("GET Element: %s\n", host);
    //locks the queue
    pthread_mutex_lock(&m_table);
    
    //check if the host exists
    int index = isHostPresent(host);
    if(index != -1){
        //the host exists in the table return the values
        *element = table[index];
    }
    //unlocks the queue
    pthread_mutex_unlock(&m_table);    
    // printf("GET Element: %s result=%d\n", host,index);
    return index;
}

int updateConnection(int index, int sock, int conn){
    pthread_mutex_lock(&m_table);
    table[index].connection[conn].sock = sock;
    table[index].connection[conn].used = 1;
    table[index].connection[conn].inuse = 0;
    pthread_mutex_unlock(&m_table); 
}

int updateElement(int index, int conn){
    pthread_mutex_unlock(&m_table);
    table[index].connection[conn].used = 1;
    table[index].connection[conn].inuse = 0;
    pthread_cond_broadcast(&table[index].cv);
    pthread_mutex_unlock(&m_table);
}

int removeElement(char *host, int conn){
    int i;
    pthread_mutex_lock(&m_table);
    int index = isHostPresent(host);   
    if(index != -1){
        if(!table[index].connection[0].inuse && !table[index].connection[1].inuse){  
            for(i= index; i < MAX_REQ-1; table[i] = table[i+1], i++);
            table_size--;
        }else{
            table[index].connection[conn].used =0;
            table[index].connection[conn].inuse =0;
        }
        pthread_cond_broadcast(&table[index].cv);
    }
    pthread_mutex_unlock(&m_table);

    return index;
}


