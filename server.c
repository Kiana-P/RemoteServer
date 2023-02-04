#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include <ctype.h>

#define BACKLOG 5
#define BUFSIZE 1

struct Node {
    char *key;
    char *value;
    struct Node *next;
};

struct LList {
    struct Node *front;
    int open;
    unsigned count;
    pthread_mutex_t lock;
    pthread_cond_t read_ready;
    pthread_cond_t write_ready;
};

struct connection{
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int fd;
    struct LList *l;
};

struct LList *createLList();
struct Node *newNode(char *k, char *v);
int destroy(struct LList *l);
int printL(struct LList *l);
int set(struct LList *l, char *k, char* v);
char* get(struct LList *l, char *k);
char* del(struct LList *l, char *k);
void *worker(void *arg);    //thread function

//global variables
char KNF[4] = "KNF\n";

int main(int argc, char **argv) {

    //server program here
    if (argc != 2) {
        printf("Usage: %s [port]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct addrinfo hint, *info_list, *info;
    struct connection *con;
    int error, sfd;
    pthread_t tid;
    struct LList *myLL = createLList();

    // initialize hints
    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;     //listening socket

    // get socket and address info for listening port
    error = getaddrinfo(NULL, argv[1], &hint, &info_list);
    if (error != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        exit(EXIT_FAILURE);
    }

    // attempt to create socket
    for (info = info_list; info != NULL; info = info->ai_next) {
        sfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        
        // if we couldn't create the socket, try the next method
        if (sfd == -1) {
            continue;
        }

        // if we were able to create the socket, try to set it up for incoming connections;
        // note that this requires two steps:
        // - bind associates the socket with the specified port on the local host
        // - listen sets up a queue for incoming connections and allows us to use accept
        if ((bind(sfd, info->ai_addr, info->ai_addrlen) == 0) && (listen(sfd, BACKLOG) == 0)) {
            break;
        }

        // unable to set it up, so try the next method
        close(sfd);
    }

    if (info == NULL) {
        // we reached the end of result without successfuly binding a socket
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    freeaddrinfo(info_list);

    // at this point sfd is bound and listening
    printf("Waiting for connection\n");
    for (;;) {
        // create argument struct for child thread
        con = malloc(sizeof(struct connection));
        con->l = myLL;
        con->addr_len = sizeof(struct sockaddr_storage);
            // addr_len is a read/write parameter to accept
            // we set the initial value, saying how much space is available
            // after the call to accept, this field will contain the actual address length
        
        // wait for an incoming connection
        con->fd = accept(sfd, (struct sockaddr *) &con->addr, &con->addr_len);
            // we provide
            // sfd - the listening socket
            // &con->addr - a location to write the address of the remote host
            // &con->addr_len - a location to write the length of the address
            //
            // accept will block until a remote host tries to connect
            // it returns a new socket that can be used to communicate with the remote
            // host, and writes the address of the remote hist into the provided location
        
        // if we got back -1, it means something went wrong
        if (con->fd == -1) {
            perror("accept");
            continue;
        }

        // spin off a worker thread to handle the remote connection
        error = pthread_create(&tid, NULL, worker, con);

        // if we couldn't spin off the thread, clean up and wait for another connection
        if (error != 0) {
            fprintf(stderr, "Unable to create thread: %d\n", error);
            close(con->fd);
            free(con);
            continue;
        }

        // otherwise, detach the thread and wait for the next connection request
        pthread_detach(tid);
    }

    //never reach here

        /* Linked List testing
    	struct LList *myLL = createLList();
    	int check = 0;
    	char name[4] = "woo";
    	char name2[5] = "poot";
    	char name3[6] = "toott";
    	char name4[7] = "woot";
    	check = set(myLL, name, name2);
        check = printL(myLL);
    	check = set(myLL, name2, name4);
        check = printL(myLL);
    	check = set(myLL, name3, name);
        check = printL(myLL);
    	check = set(myLL, name4, name2);
        check = printL(myLL);
        printf("%s\n", get(myLL, "woo"));
        printf("%s\n", del(myLL, "woot"));
        printf("%s\n", get(myLL, "woot"));

        check = destroy(myLL);
        */

}

void *worker(void *arg){    //write worker function
    char host[100], port[10], buf[BUFSIZE + 1];
    struct connection *c = (struct connection *) arg;
    int error, r;

    // find out the name and port of the remote host
    error = getnameinfo((struct sockaddr *) &c->addr, c->addr_len, host, 100, port, 10, NI_NUMERICSERV);
        // we provide:
        // the address and its length
        // a buffer to write the host name, and its length
        // a buffer to write the port (as a string), and its length
        // flags, in this case saying that we want the port as a number, not a service name
    if (error != 0) {
        fprintf(stderr, "getnameinfo: %s", gai_strerror(error));
        close(c->fd);
        return NULL;
    }

    printf("[%s:%s] connection\n", host, port);

    while(1){

        //variables
        char lenERR[8] = "ERR\nLEN\n";
        char srvERR[8] = "ERR\nSRV\n";
        char badERR[8] = "ERR\nBAD\n";
        char OKS[4] = "OKS\n";
        char OKG[4] = "OKG\n";
        char OKD[4] = "OKD\n";
        char end[2];
        char* message = malloc(sizeof(char) * 4);
        int count = 0;
        int n = 0;
        int numlen = 2;
        int keylen = 2;
        int vallen = 2;
        int newln = 0;
        char* num = malloc(sizeof(char) * 2);
        char* key = malloc(sizeof(char) * 2);
        char* value = malloc(sizeof(char) * 2);

        //get message
        for(int i=0; i<4; i++){
            r = read(c->fd, buf, BUFSIZE);
            if(i == 0){
                if(buf[0] == 'G'){
                    message = "GET\n";
                    //write(c->fd, message, 4);
                }
                else if (buf[0] == 'S'){
                    message = "SET\n";
                    //write(c->fd, message, 4);
                }
                else if (buf[0] == 'D'){
                    message = "DEL\n";
                    //write(c->fd, message, 4);
                }
                else{
		          write(c->fd, badERR, 8);
                    free(num);
                    free(key);
                    free(value);
                    close(c->fd);
                    return NULL;
                }
            }
            else if((i == 3) && (message[i] != '\n')){
	           write(c->fd, badERR, 8);
                free(num);
                free(key);
                free(value);
                close(c->fd);
                return NULL;
            }
            else if(message[i] != buf[0]){
	           write(c->fd, badERR, 8);
                free(num);
                free(key);
                free(value);
                close(c->fd);
                return NULL;
            }

        }

        //read number
        do{
            r = read(c->fd, buf, BUFSIZE);
            if(isdigit(buf[0])>0){
                if(count>=numlen){
                    numlen = numlen+1;
                    char *p = realloc(num, sizeof(char) * (numlen));
                    if (!p){
                        write(c->fd, srvERR, 8);
                        free(num);
                        free(key);
                        free(value);
                        close(c->fd);
                        return NULL;
                    }
                    num = p;
                }
                num[count] = buf[0];
                count++;
            }
            else if(buf[0] != '\n'){
                write(c->fd, badERR, 8);
                free(num);
                free(key);
                free(value);
                close(c->fd);
                return NULL;
            } 

        }while(buf[0] != '\n');

        n = atoi(num);
        free(num);
        count = 0;
        newln = 0;

        if(strcmp(message, "SET\n") == 0){
            for(int i=0; i<n; i++){
                r = read(c->fd, buf, BUFSIZE);
                if(newln == 0){
                    if(i>=(keylen)){
                        keylen = keylen+1;
                        char *p = realloc(key, sizeof(char) * (keylen));
                        if (!p){
                            write(c->fd, srvERR, 8);
                            free(key);
                            free(value);
                            close(c->fd);
                            return NULL;
                        }
                        key = p;
                    }
                    key[i] = buf[0];
                    if(key[i] == '\n'){
                        newln = 1;
                        if((i+1)>=(keylen)){
                            keylen = keylen+1;
                            char *p = realloc(key, sizeof(char) * (keylen));
                            if (!p){
                                write(c->fd, srvERR, 8);
                                free(key);
                                free(value);
                                close(c->fd);
                                return NULL;
                            }
                            key = p;
                        }
                        key[i+1] = '\0';
                    }
                }
                else{
                    //printf("%d %d\n", count, vallen);
                    if(count>=(vallen)){
                        vallen = vallen+1;
                        char *p = realloc(value, sizeof(char) * (vallen));
                        if (!p){
                            write(c->fd, srvERR, 8);
                            free(key);
                            free(value);
                            close(c->fd);
                            return NULL;
                        }
                        value = p;
                    }
                    value[count] = buf[0];
                    //write(c->fd, value, vallen);
                    //printf("here\n");
                    if((i < n-1) && (value[count] == '\n')){
                        write(c->fd, lenERR, 8);
                        free(key);
                        free(value);
                        close(c->fd);
                        return NULL;
                    }
                    else if((i == n-1) && (value[count] != '\n')){
                        write(c->fd, lenERR, 8);
                        free(key);
                        free(value);
                        close(c->fd);
                        return NULL;
                    }
                    if(value[count] == '\n'){
                        if((count+1)>=(vallen)){
                            vallen = vallen+1;
                            char *p = realloc(value, sizeof(char) * (vallen));
                            if (!p){
                                write(c->fd, srvERR, 8);
                                free(key);
                                free(value);
                                close(c->fd);
                                return NULL;
                            }
                            value = p;
                        }
                        value[count+1] = '\0';
                    }
                    count++;
                }
            }
            //write(c->fd, key, keylen);
            //write(c->fd, value, vallen);
            count = set(c->l, key, value);
            if(count == 0){
                write(c->fd, OKS, 4);
            }

        }
        else{
            //read data one byte at a time for message
            for(int i=0; i<n; i++){
                r = read(c->fd, buf, BUFSIZE);
                if((i < n-1) && (buf[0] == '\n')){
                    write(c->fd, lenERR, 8);
                    free(key);
                    free(value);
                    close(c->fd);
                    return NULL;
                }
                else if((i == n-1) && (buf[0] != '\n')){
                    write(c->fd, lenERR, 8);
                    free(key);
                    free(value);
                    close(c->fd);
                    return NULL;
                }
                //write(c->fd, buf, 1);
                if(i>=(keylen-1)){
                    keylen = keylen+1;
                    char *p = realloc(key, sizeof(char) * (keylen));
                    if (!p){
                        write(c->fd, srvERR, 8);
                        free(key);
                        free(value);
                        close(c->fd);
                        return NULL;
                    }
                    key = p;
                }
                key[i] = buf[0];
                if(key[i] == '\n'){
                    if((i+1)>=(keylen)){
                        keylen = keylen+1;
                        char *p = realloc(key, sizeof(char) * (keylen));
                        if (!p){
                            write(c->fd, srvERR, 8);
                            free(key);
                            free(value);
                            close(c->fd);
                            return NULL;
                        }
                        key = p;
                    }
                    key[i+1] = '\0';
                }
             
            }

            if(strcmp(message, "GET\n") == 0){
                //printf("in here lol\n");
                char *retstr = get(c->l, key);
                char buffr[20];
                int retlen = strlen(retstr);
                int lenused = sprintf(buffr, "%d", retlen);
                char newln[1];
                newln[0] = '\n';
                if(strcmp(retstr, "KNF\n") == 0){
                    write(c->fd, retstr, 4);
                }
                else{
                    write(c->fd, OKG, 4);
                    write(c->fd, buffr, lenused);
                    write(c->fd, newln, 1);
                    write(c->fd, retstr, retlen);

                }
            }
            else{
                char *retstr = del(c->l, key);
                char buffr[20];
                int retlen = strlen(retstr);
                int lenused = sprintf(buffr, "%d", retlen);
                char newln[1];
                newln[0] = '\n';
                if(strcmp(retstr, "KNF\n") == 0){
                    write(c->fd, retstr, 4);
                }
                else{
                    write(c->fd, OKD, 4);
                    write(c->fd, buffr, lenused);
                    write(c->fd, newln, 1);
                    write(c->fd, retstr, retlen);
                }   
            }
        }
    }
    //never get here

    printf("[%s:%s] got EOF\n", host, port);

    close(c->fd);
    free(c);
    return NULL;

}

// function to create a new linked list node.
struct Node *newNode(char *k, char *v) {
    struct Node *temp = (struct Node *) malloc(sizeof(struct Node));
    temp->key = malloc(strlen(k)+1); 
    temp->value = malloc(strlen(v)+1);
    strcpy(temp->key, k);
    strcpy(temp->value, v);
    temp->next = NULL;
    return temp;
}

// function to create an empty linked list
struct LList *createLList() {
    struct LList *l = (struct LList *) malloc(sizeof(struct LList));
    l->front = NULL;
    l->count = 0;
    l->open = 1;
    pthread_mutex_init(&l->lock, NULL);
    pthread_cond_init(&l->read_ready, NULL);
    pthread_cond_init(&l->write_ready, NULL);
    return l;
}

int destroy(struct LList *l) {

    //pthread_mutex_lock(&l->lock);

    pthread_mutex_destroy(&l->lock);
    pthread_cond_destroy(&l->read_ready);
    pthread_cond_destroy(&l->write_ready);

    if(l->front == NULL){
        free(l);
        return 0;
    }

    //free everything
    struct Node *curr = l->front;
    struct Node *nxt = l->front->next;
    while(curr != NULL){
        
        //free words
        free(curr->key);
        free(curr->value);
        //free node
        free(curr);

        curr = nxt;
        if(nxt == NULL){
            break;
        }
        else{
            nxt = nxt->next;
        }
    }

    free(l);
    return 0;
}

int set(struct LList *l, char *k, char *v){
	//printf("begin insert\n");
    pthread_mutex_lock(&l->lock);
    
    // Create a new LL node
    if (!l->open) {
        pthread_mutex_unlock(&l->lock);
        return -1;
    }
    
    struct Node *temp = newNode(k, v);
    struct Node *curr = l->front;
    struct Node *prev = curr;

    // If new node needs to be in front
    if ((curr == NULL) || (strcmp(temp->key, curr->key) <= 0)) {

        if(curr == NULL){
            l->front = temp;
            l->count++;
        }
        else if(strcmp(temp->key, curr->key) == 0){  //if key already exists, change value
            curr->value = temp->value;
            free(temp->key);
            free(temp->value);
            free(temp);
        }
        else{   //if key needs to be at front
            l->front = temp;
            temp->next = curr;
            l->count++;
        }

        pthread_cond_signal(&l->read_ready);
        pthread_mutex_unlock(&l->lock);
        return 0;
    }

    // Add the new node in middle or end
    while(curr != NULL){

        if(strcmp(temp->key, curr->key) <= 0){
            if(strcmp(temp->key, curr->key) == 0){
                curr->value = temp->value;
                free(temp->key);
                free(temp->value);
                free(temp);
            }
            else{
                prev->next = temp;
                temp->next = curr;
                l->count++;
            }
            break;
        }
    	
        //if true, put new node at end of list
    	if(curr->next == NULL){
    		curr->next = temp;
    		l->count++;
    		break;
    	} 

        prev = curr;
    	curr = curr->next;
    }

    pthread_cond_signal(&l->read_ready);
    pthread_mutex_unlock(&l->lock);
    return 0;
}

char* get(struct LList *l, char *k){

    pthread_mutex_lock(&l->lock);

    struct Node *temp = l->front;
    while(temp != NULL){

        if(strcmp(temp->key, k) == 0){
            pthread_cond_signal(&l->read_ready);
            pthread_mutex_unlock(&l->lock);
            return temp->value;
        }
        
        temp = temp->next;
    }

    //if here the key is not in the list

    pthread_cond_signal(&l->read_ready);
    pthread_mutex_unlock(&l->lock);
    return KNF;


}

char* del(struct LList *l, char *k){

    pthread_mutex_lock(&l->lock);

    struct Node *curr = l->front;
    struct Node *prev = NULL;
    char* temp;
    while(curr != NULL){

        if(strcmp(curr->key, k) == 0){

            temp = curr->value;
            l->count--;

            if(curr == l->front){   //key is found at front
                l->front = curr->next;
                curr = curr->next;

            }
            else if(curr->next == NULL){     //key is found at the end
                prev->next = NULL;
            }
            else{   //key is found in middle
                prev->next = curr->next;
            }

            pthread_cond_signal(&l->read_ready);
            pthread_mutex_unlock(&l->lock);
            return temp;
        }
        
        prev = curr;
        curr = curr->next;
    }

    //if here the key is not in the list

    pthread_cond_signal(&l->read_ready);
    pthread_mutex_unlock(&l->lock);
    return KNF;

}

int printL(struct LList *l){
    pthread_mutex_lock(&l->lock);

    struct Node *temp = l->front;
    while(temp != NULL){

        printf("PRINT LIST: %s %s \n", temp->key, temp->value);
        
        temp = temp->next;
    }

    //if here the key is not in the list

    pthread_cond_signal(&l->read_ready);
    pthread_mutex_unlock(&l->lock);
    return 0;
}
