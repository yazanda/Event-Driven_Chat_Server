#include "chatServer.h"
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>


#define FUNCTION_FAIL (-1)
#define FUNCTION_SUCCESS 0

#define MAX_PORT 65536
static int end_server = 0;

void destroy_pool(const int*, conn_pool_t*);

int isNumber(char *txt){
    for (int i = 0; i < strlen(txt); i++) {
        if((int)txt[i] < 48 || (int)txt[i] > 57)
            return FUNCTION_FAIL;
    }
    return FUNCTION_SUCCESS;
}

void intHandler(int SIG_INT) {
    /* use a flag to end_server to break the main loop */
    if(SIG_INT == SIGINT)
    end_server = 1;
}

int main (int argc, char *argv[])
{
    //parsing input.
    if(argc != 2){
        printf("Usage: server <port>");
        exit(EXIT_FAILURE);
    }
    if(isNumber(argv[1]) == FUNCTION_FAIL){
        printf("incorrect port input!\n");
        exit(EXIT_FAILURE);
    }
    int port = (int)strtol(argv[1], NULL, 10);
    if(!(port > 0 && port <= MAX_PORT)){
        printf("incorrect port input!\n");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, intHandler);

    conn_pool_t* pool = (conn_pool_t*) calloc(1, sizeof(conn_pool_t));
    init_pool(pool);

    /*************************************************************/
    /* Create an AF_INET stream socket to receive incoming      */
    /* connections on                                            */
    /*************************************************************/
    //socket(...);
    int socket_fd;
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY);
    server.sin_port = htons(port);
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }
    /*************************************************************/
    /* Set socket to be nonblocking. All the sockets for      */
    /* the incoming connections will also be nonblocking since   */
    /* they will inherit that state from the listening socket.   */
    /*************************************************************/
    //ioctl(...);
    int on = 1;
    if(ioctl(socket_fd, (int)FIONBIO, (char *)&on) < 0){
        perror("ioctl() failed");
        exit(EXIT_FAILURE);
    }
    /*************************************************************/
    /* Bind the socket                                           */
    /*************************************************************/
    //bind(...);
    if(bind(socket_fd, (struct sockaddr *) &server, sizeof(server)) < 0){
        perror("bind() failed");
        exit(EXIT_FAILURE);
    }
    /*************************************************************/
    /* Set the listen backlog                                   */
    /*************************************************************/
    //listen(...);
    if(listen(socket_fd, 5) < 0){
        perror("listen() failed");
        exit(EXIT_FAILURE);
    }
    /*************************************************************/
    /* Initialize fd_sets  			                             */
    /*************************************************************/
    FD_SET(socket_fd, &pool->read_set);
    pool->maxfd = socket_fd;
    /*************************************************************/
    /* Loop waiting for incoming connects, for incoming data or  */
    /* to write data, on any of the connected sockets.           */
    /*************************************************************/
    int currentMax;

    do {
        /**********************************************************/
        /* Copy the master fd_set over to the working fd_set.     */
        /**********************************************************/
        FD_ZERO(&pool->ready_read_set);
        FD_ZERO(&pool->ready_write_set);
        memcpy(&pool->ready_read_set, &pool->read_set, sizeof(pool->read_set));
        memcpy(&pool->ready_write_set, &pool->write_set, sizeof(pool->write_set));
        /**********************************************************/
        /* Call select() 										  */
        /**********************************************************/
        //select(...);
        printf("Waiting on select()...\nMaxFd %d\n", pool->maxfd);
        int readySockets = select(pool->maxfd + 1, &pool->ready_read_set, &pool->ready_write_set, 0, 0);
        if(readySockets < 0){
            break;
        }
        pool->nready = readySockets;
        /**********************************************************/
        /* One or more descriptors are readable or writable.      */
        /* Need to determine which ones they are.                 */
        /**********************************************************/
        currentMax = pool->maxfd;
        int countReady = 0;
        for (int fd = 3; fd <= currentMax && countReady != pool->nready; fd++) {
            /* Each time a ready descriptor is found, one less has  */
            /* to be looked for.  This is being done so that we     */
            /* can stop looking at the working set once we have     */
            /* found all the descriptors that were ready         */

            /*******************************************************/
            /* Check to see if this descriptor is ready for read   */
            /*******************************************************/
            if (FD_ISSET(fd, & pool->ready_read_set)) {
                countReady++;
                /***************************************************/
                /* A descriptor was found that was readable		   */
                /* if this is the listening socket, accept one      */
                /* incoming connection that is queued up on the     */
                /*  listening socket before we loop back and call   */
                /* select again. 						            */
                /****************************************************/
                //accept(...)
                if(fd == socket_fd){
                    int sd = accept(socket_fd, NULL, NULL);
                    if(sd <= 0) {
                        FD_CLR(sd, &pool->ready_read_set);
                        continue;
                    }
                    printf("New incoming connection on sd %d\n", sd);
                    if(add_conn(sd, pool) == FUNCTION_FAIL){
                        continue;
                    }
                    FD_CLR(sd, &pool->ready_read_set);
                }
                    /****************************************************/
                    /* If this is not the listening socket, an 			*/
                    /* existing connection must be readable				*/
                    /* Receive incoming data his socket             */
                    /****************************************************/
                    //read(...)
                else {
                    char buffer[BUFFER_SIZE];
                    bzero(buffer, sizeof(buffer));
                    printf("Descriptor %d is readable\n", fd);
                    size_t reader = read(fd, buffer, BUFFER_SIZE);
                    /* If the connection has been closed by client 		*/
                    /* remove the connection (remove_conn(...))    		*/
                    if(reader == 0){
                        remove_conn(fd, pool);
                        printf("Connection closed for sd %d\n",fd);
                    } else if((int)reader < 0) {
                        continue;
                    } else {
                        /**********************************************/
                        /* Data was received, add msg to all other    */
                        /* connections					  			  */
                        /**********************************************/
                        //add_msg(...);
                        printf("%d bytes received from sd %d\n", (int)reader, fd);
                        add_msg(fd, buffer ,(int)reader, pool);
                    }

                }
            } /* End of if (FD_ISSET()) */
            /*******************************************************/
            /* Check to see if this descriptor is ready for write  */
            /*******************************************************/
            if (FD_ISSET(fd, &pool->ready_write_set)) {
                countReady++;
                /* try to write all msgs in queue to sd */
                //write_to_client(...);
                if(write_to_client(fd, pool) == FUNCTION_FAIL)
                    continue;
            }
            /*******************************************************/


        } /* End of loop through selectable descriptors */

    } while (end_server == 0);

    /*************************************************************/
    /* If we are here, Control-C was typed,						 */
    /* clean up all open connections					         */
    /*************************************************************/
    destroy_pool(&socket_fd, pool);
    return 0;
}


int init_pool(conn_pool_t* pool) {
    //initialized all fields
    if(pool == NULL){
        printf("pool isn't allocated");
        return FUNCTION_FAIL;
    }
    pool->maxfd = 0;
    pool->nready = 0;
    FD_ZERO(&pool->read_set);
    FD_ZERO(&pool->write_set);
    pool->conn_head = NULL;
    pool->nr_conns = 0;
    return 0;
}

int add_conn(int sd, conn_pool_t* pool) {
    /*
     * 1. allocate connection and init fields
     * 2. add connection to pool
     * */
    conn_t *connection = (conn_t*) calloc(1, sizeof(conn_t));
    if(!connection){
        perror("allocation memory failed");
        return FUNCTION_FAIL;
    }
    connection->prev = NULL;
    connection->next = NULL;
    connection->write_msg_head = NULL;
    connection->write_msg_tail = NULL;
    connection->fd = sd;
    FD_SET(sd, & pool->read_set);
    conn_t *current = pool->conn_head;
    if(current == NULL){
        pool->conn_head = connection;
    } else {
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = connection;
        connection->prev = current;
    }
    pool->nr_conns++;
    if(pool->maxfd < sd)
        pool->maxfd = sd;
    return FUNCTION_SUCCESS;
}


int remove_conn(int sd, conn_pool_t* pool) {
    /*
    * 1. remove connection from pool
    * 2. deallocate connection
    * 3. remove from sets
    * 4. update max_fd if needed
    */
    if(pool->nr_conns == 0){
        return FUNCTION_FAIL;
    }
    conn_t *current = pool->conn_head;
    while (current->fd != sd){
        current = current->next;
    }
    if(current == NULL){
        return FUNCTION_FAIL;
    }
    if(sd == pool->maxfd){
        pool->maxfd = 3;
        conn_t *temp = pool->conn_head;
        while (temp) {
            if(temp->fd > pool->maxfd && temp->fd != sd){
                pool->maxfd = temp->fd;
            }
            temp = temp->next;
        }
    }

    msg_t *currentMsg = current->write_msg_head;
    msg_t *tempMsg = NULL;
    while (currentMsg != NULL){
        tempMsg = currentMsg->next;
        free(currentMsg->message);
        free(currentMsg);
        currentMsg = tempMsg;
    }
    tempMsg = NULL;
    if(pool->nr_conns == 1) {
        pool->conn_head = NULL;
    }
    else if(current == pool->conn_head){
        pool->conn_head = pool->conn_head->next;
        pool->conn_head->prev = NULL;
    } else{
        current->prev->next = current->next;
        current->next->prev = current->prev;
    }
    free(current);
    current = NULL;
    pool->nr_conns--;
    FD_CLR(sd, &pool->read_set);
    FD_CLR(sd, &pool->write_set);
    close(sd);
    return FUNCTION_SUCCESS;
}

int add_msg(int sd,char* buffer,int len,conn_pool_t* pool) {

    /*
     * 1. add msg_t to write queue of all other connections
     * 2. set each fd to check if ready to write
     */
    conn_t *connection = pool->conn_head;
    while (connection) {
        if (connection->fd != sd) {
            msg_t *message = (msg_t *) calloc(1, sizeof(msg_t));
            if (!message) {
                perror("allocation failed");
                return FUNCTION_FAIL;
            }
            message->prev = NULL;
            message->next = NULL;
            message->message = (char *) calloc(len + 1, sizeof(char));
            strncpy(message->message, buffer, len);
            message->size = len;
            msg_t *currentMsg = connection->write_msg_head;
            if (currentMsg == NULL) {
                connection->write_msg_head = message;
                connection->write_msg_tail = message;
            } else {
                connection->write_msg_tail->next = message;
                message->prev = connection->write_msg_tail;
                connection->write_msg_tail = message;
            }
            FD_SET(connection->fd, &pool->write_set);
        }
        connection = connection->next;
    }
    return FUNCTION_SUCCESS;
}

int write_to_client(int sd,conn_pool_t* pool) {

    /*
     * 1. write all msgs in queue
     * 2. deallocate each writen msg
     * 3. if all msgs were writen successfully, there is nothing else to write to this fd... */
    if(pool == NULL || pool->nr_conns == 0){
        return FUNCTION_FAIL;
    }
    int conn_found = 0;
    conn_t *connection = pool->conn_head;
    while (connection != NULL){
        if(connection->fd == sd){
            conn_found = 1;
            break;
        }
        connection = connection->next;
    }
    if(!conn_found)
        return FUNCTION_FAIL;
    if(!connection->write_msg_head)
        return FUNCTION_FAIL;
    msg_t *tempMsg = connection->write_msg_head;
    while (tempMsg){
        if(write(sd, tempMsg->message, tempMsg->size) < 0){
            perror("write failed");
        }
        free(tempMsg->message);
        tempMsg->message = NULL;
        tempMsg = tempMsg->next;
    }
    tempMsg = connection->write_msg_head;
//    msg_t *temp = NULL;
    while (tempMsg){
        connection->write_msg_head = connection->write_msg_head->next;
        free(tempMsg);
        tempMsg = connection->write_msg_head;
//        temp = tempMsg;
//        tempMsg = temp->next;
//        free(temp);
//        temp = NULL;
    }
//    connection->write_msg_head = NULL;
//    connection->write_msg_tail = NULL;
    FD_CLR(sd, &pool->write_set);
    return FUNCTION_SUCCESS;
}

void destroy_pool(const int *socket_fd,conn_pool_t* pool){
    conn_t *current = pool->conn_head;
    conn_t *temp = NULL;
    while(current){
        temp = current->next;
        printf("removing connection with sd %d \n", current->fd);
        remove_conn(current->fd, pool);
        current = temp;
    }
    FD_CLR(*socket_fd, &pool->read_set);
    FD_CLR(*socket_fd, &pool->write_set);
    FD_CLR(*socket_fd, &pool->ready_read_set);
    FD_CLR(*socket_fd, &pool->ready_write_set);
    close(*socket_fd);
    free(pool);
}