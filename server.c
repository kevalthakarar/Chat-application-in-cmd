#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>

#define MAX_CLIENT 100
int uid = 10;
#define BUFFER_SZ 2048
#define name_length 50

static _Atomic unsigned int cli_count = 0;
char *mode;



typedef struct{         // client information

    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[name_length];
}client_t;

client_t *clients[MAX_CLIENT];

pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;


void str_trim_lf (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // trim \n
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}
void client_add(client_t* cl){      // just keep track of active client
    pthread_mutex_lock(&client_mutex);

    for(int i = 0;i<MAX_CLIENT;i++){
        if(!clients[i]){
            clients[i] = cl;
            break;
        }
    }

    pthread_mutex_unlock(&client_mutex);
}


void client_remove(int uid){
    pthread_mutex_lock(&client_mutex);

    for(int i = 0;i<MAX_CLIENT;i++){
        if(clients[i] && clients[i]->uid == uid){       
            clients[i] = NULL;
            break;
        }
    }

    pthread_mutex_unlock(&client_mutex);
}

void brodcast_send(char* s , int uid){
    pthread_mutex_lock(&client_mutex);

    for(int i = 0;i<MAX_CLIENT;i++){
        if(clients[i] && clients[i]->uid != uid){
            if(write(clients[i]->sockfd , s , strlen(s)) < 0){
                printf("Erore in writing discripter\n");
                break;
            }
        }
    }

    pthread_mutex_unlock(&client_mutex);
}
void onetone(char* s , int uid){
    pthread_mutex_lock(&client_mutex);
    int index = -1;
    for(int i = 0;i<MAX_CLIENT;i++){
        if(clients[i] && clients[i]->uid == uid){
            index = i;
        }
    }

    int i= index+1;
    while(i != index){
        
        if(clients[i]){
            if(write(clients[i]->sockfd , s , strlen(s)) < 0){
                printf("Eroror in writing discriptor\n");
            }
            break;
        }
        i = (i+1) % MAX_CLIENT;
    }

    pthread_mutex_unlock(&client_mutex);
}


void* handle_client(void* arg){

    char buf_out[BUFFER_SZ];
    char name[name_length];

    int flag_leave = 0;
    client_t *cli = (client_t*) arg;

    // recieving name of client
    if(recv(cli -> sockfd , name , name_length , 0) <= 0){
        printf("Didn't enter name\n");
        flag_leave = 1;
    }
    else{
        strcpy(cli->name, name);
        sprintf(buf_out,"%s is onine",cli->name);
        printf("%s is online\n",cli->name);
        brodcast_send(buf_out , cli->uid);
    }
    bzero(buf_out , BUFFER_SZ);



    while(1){
        if(flag_leave)
            break;

        int receve = recv(cli->sockfd , buf_out , BUFFER_SZ , 0);
        
        if(receve > 0){
            char *ack = "Server : i got your message";
                int ackno = write(cli->sockfd , ack ,strlen(ack));
                if(ackno <0){
                    printf("Erore in writing message\n");
                }
                if(strlen(buf_out) > 0 ){
                    if(mode[0] == 'B' || mode[0] == 'b'){
                    brodcast_send(buf_out , cli->uid);
				    printf("%s \n", buf_out);
                    }
                    else{
                        onetone(buf_out , cli->uid);
				        printf("%s \n", buf_out);
                    }
                }
        } 
        else if(receve == 0){   // for connection is lost
            sprintf(buf_out ,"%s has left",cli->name);
            printf("%s has left metting\n",cli->name);
            brodcast_send(buf_out , cli->uid);
            flag_leave = 1;
        }
        else{
            printf("Erorr in receve \n");
            flag_leave = 1;
        }
        bzero(buf_out , BUFFER_SZ);
    }
    close(cli->sockfd);
    client_remove(cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

	return NULL;
} 

int main(int argc , char ** argv){

    if(argc <= 2){
        printf("Usage %s <port>\n",argv[0]);
        printf("Enter broadcast for b and onetoone for o\n");
        return EXIT_FAILURE;
    }

    char *ip = "127.0.0.1";
	int port = atoi(argv[1]);
    mode  = argv[2];
    printf("%s ",mode);
	int option = 1;
	int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    /* Socket settings */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    // ignoring pipe signal
    signal(SIGPIPE , SIG_IGN);

    // setting the option for socket
    if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
		perror("ERROR: setsockopt failed");
        return EXIT_FAILURE;
	}

    // Binding
    if(bind(listenfd , (struct sockaddr*)&serv_addr , sizeof(serv_addr)) < 0 ){
        printf("Erro in binding \n");
        return EXIT_FAILURE;
    }

    // listen
    if(listen(listenfd , 10) < 0){
        printf("Eror in listening\n");
        return EXIT_FAILURE;        
    }

    printf("Server start successfuly \n");
    while(1){
        socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);



        if(cli_count == MAX_CLIENT){
            char *ma = "Max client Reached ";
            int maxc = write(connfd , ma , strlen(ma));
            if(maxc < 0){
                printf("Eroror in writing\n");
            }
            printf("MAX Client Reached \n");
            close(connfd);
            continue;       
        }
        cli_count++;
        client_t* cli = (client_t *)malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;
        client_add(cli);
        pthread_create(&tid , NULL , &handle_client , (void *)cli);

    }

    return EXIT_SUCCESS;




}
