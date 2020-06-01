#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>

#define SERVER_PORT 6666
#define MAX_CLIENT_NUM  32

struct client_t {
    pthread_t thread;
    int sockfd;
};

struct client_t clients[MAX_CLIENT_NUM];

int sockfd;
pthread_mutex_t mutex;

void *request_handler(void *sockfd);
void sigint_handler(int num);
char *get_time();
struct client_t *get_client();
void send_all(char buffer[], int length);
void init_client();
void clear_client(pthread_t thread);
void delete_clients();

int main(int argc, char *argv[]){
    // set sigint handler
    signal(SIGINT, sigint_handler);
    // ignore sigpipe
    signal(SIGPIPE, SIG_IGN);

    // init all clients
    init_client();

    // init mutex
    int init_res = pthread_mutex_init(&mutex, NULL);
    assert(init_res == 0);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(sockfd != -1);

    // bind
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    serv_addr.sin_port = htons(SERVER_PORT);
    assert(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != -1);

    // listen
    assert(listen(sockfd, 20) != -1);

    printf("run tcp server in %s:%d ......\n", "127.0.0.1", SERVER_PORT);
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_size = sizeof(clnt_addr);
    while (1){
        int cfd = accept(sockfd, (struct sockaddr *)&clnt_addr, &clnt_addr_size);

        printf("[%s] connect from %s:%d  socket: %d\n", get_time(), inet_ntoa(clnt_addr.sin_addr), clnt_addr.sin_port, cfd);
        
        struct client_t *c = get_client();
        if(c == NULL){
            close(cfd);
            continue;
        }
        // create a new thread to connect to client
        pthread_t thread;
        int res = pthread_create(&thread, NULL, request_handler, (void*)(&cfd));

        if(res == 0){
            c->thread = thread;
            c->sockfd = cfd;
        }else{
            close(cfd);
        }
    }

    close(sockfd);
    return 0;
}

void *request_handler(void* sockfd){
    int fd = *((int*)sockfd);
    printf("current thread: %ld socket: %d\n", pthread_self(), fd);
    char buffer[1024];
    int n = -1;
    while ((n = read(fd, buffer, 1024)) > 0){
        send_all(buffer, n);
    }
    close(fd);
    clear_client(pthread_self());
    pthread_exit(NULL);
}

void sigint_handler(int num){
    close(sockfd);
    delete_clients();
    pthread_mutex_destroy(&mutex);
    exit(0);
}

char *get_time(){
    time_t t = time(NULL);
    char *time = ctime(&t);
    char *res = time;
    while (*time != '\n')
    {
        time++;
    }
    *time = '\0';
    return res;
}

struct client_t* get_client(){
    struct client_t *client = NULL;
    pthread_mutex_lock(&mutex);
    for(int i=0; i<MAX_CLIENT_NUM; i++){
        if(clients[i].sockfd == 0){
            client = &clients[i];
        }
    }
    pthread_mutex_unlock(&mutex);
    return client;
}

void send_all(char buffer[], int length){
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_CLIENT_NUM; i++){
        if(clients[i].sockfd != 0 && clients[i].thread != pthread_self()){
            write(clients[i].sockfd, buffer, length);
            write(clients[i].sockfd, "\r\n", 2);
        }
    }
    pthread_mutex_unlock(&mutex);
}


void init_client(){
    for (int i = 0; i < MAX_CLIENT_NUM; i++){
        clients[i].sockfd = 0;
        clients[i].thread = 0;
    }   
}

void clear_client(pthread_t thread){
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < MAX_CLIENT_NUM; i++){
        if(clients[i].thread == thread){
            clients[i].sockfd = 0;
            clients[i].thread = 0;
        }
    }
    pthread_mutex_unlock(&mutex);
}

void delete_clients(){
    for (size_t i = 0; i < MAX_CLIENT_NUM; i++){
        if(clients[i].sockfd != 0){
            close(clients[i].sockfd);
        }
    }
}