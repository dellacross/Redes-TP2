#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define BUFSZ 500 //buff size

#define ERROR "ERROR"
#define OK "OK"
#define REQ_ADD "REQ_ADD"
#define RES_ADD "RES_ADD"
#define REQ_REM "REQ_REM"
#define SE_PORT 12345
#define SCII_PORT 54321

#define SE_SERVER "SE"
#define SCII_SERVER "SCII"

int client_count = 0; // Contador de clientes conectados
int clients[10] = {0};

int server_id = -1;
int production = 0;
int consumption = 0;

typedef struct {
    int production;
} SE_server;

typedef struct {
    int consumption;
} SCII_server;

void usage(int argc, char **argv) {
    // recebe o tipo de protocolo do servidor e o porto onde ficara esperando
    printf("usage: %s <v4/v6> <server port>\n", argv[0]);
    printf("example: %s v4 51551\n", argv[0]);
    exit(EXIT_FAILURE);
}

struct client_data {
    int csock; //socket do cliente
    struct sockaddr_storage storage;
};

int getClientID() {
    for(int i = 0; i < 10; i++) {
        if(clients[i] == 0) {
            clients[i] = 1;
            return i+1;
        }
    }
    return -1;
}

int checkClient(int id) {
    return clients[id-1] == 1;
}

void *client_thread(void *data) {
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);

    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);
    printf("[log] connection from %s\n", caddrstr); // printa sempre que o cliente conecta

    char* server_name;

    if(server_id == 0) server_name = SE_SERVER;
    if(server_id == 1) server_name = SCII_SERVER;

    while(1) {
        //leitura da msg do cliente
        char buf[BUFSZ];
        memset(buf, 0, BUFSZ);

        recv(cdata->csock, buf, BUFSZ-1, 0);
        printf("msg recebida: %s\n", buf);
        if(strcmp(buf, REQ_ADD) == 0) {
            int client_id = getClientID();

            if(client_id == -1) logexit("client count");

            // Envia a mensagem de resposta com o identificador
            sprintf(buf, "%s(%d)\n", RES_ADD, client_id);
            send(cdata->csock, buf, strlen(buf)+1, 0);

            printf("Client %d added\n", client_id);
        } else if(strncmp(buf, REQ_REM, strlen(REQ_REM)) == 0) {
            int cid;
            sscanf(buf, "REQ_REM(%d)", &cid);

            if(checkClient(cid) == 1) {
                clients[cid-1] = 0;
                printf("Servidor %s Client %d removed\n", server_name, cid);
                sprintf(buf, "%s(%d) - %s\n", OK, 1, server_name);
                send(cdata->csock, buf, strlen(buf)+1, 0);
                printf("msg enviada serv: %s\n", buf);
                close(cdata->csock);
                break;
            } else {
                sprintf(buf, "%s(%d)\n", ERROR, 2);
                send(cdata->csock, buf, strlen(buf)+1, 0);
            }
        } 
        else {
            printf("outra msg");
            break;
        }
    }

    close(cdata->csock);
    free(cdata);
    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    if(argc < 3) usage(argc, argv);

    //o connect recebe um ponteiro para a struct sockaddr
    struct sockaddr_storage storage;
    if(server_sockaddr_init(argv[1], argv[2], &storage) != 0) usage(argc, argv); // argv[1] -> endereco do servidor | argv[2] -> porto que recebeu | ponteiro para o storage 

    int _socket;
    _socket = socket(storage.ss_family, SOCK_STREAM, 0); //storage.ss_family -> socket (ipv4 | ipv6) | SOCK_STREAM -> SOCKET TCP
    if(_socket == -1) logexit("socket");

    // reutilizacao de porto mesmo se ja estiver sendo utilizado
    int enable = 1;
    if(setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != 0) logexit("setsockopt"); 

    //o servidor nao da connect, mas sim, bind() (depois o listen e depois o accept)

    //bind
    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if(bind(_socket, addr, sizeof(storage)) != 0) logexit("bind");

    //listen
    if(listen(_socket, 10) != 0) logexit("listen"); // 10 -> quantidade de conexoes que podem estar pendentes para tratamento (pode ser outro valor)

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    printf("bound to %s, waiting connections\n", addrstr);

    if(atoi(argv[2]) == SE_PORT) server_id = 0;
    if(atoi(argv[2]) == SCII_PORT) server_id = 1;
    if(server_id < 0 || server_id > 1) logexit("set server type");

    fd_set master;
    FD_ZERO(&master);
    FD_SET(_socket, &master);
    int fdmax = _socket;

    // tratamento das conexoes pelo while
    while(1) {
        fd_set read_fds = master;
        if(select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_storage cstorage; //client storage
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage); 

        // Verifica se a quantidade máxima de conexões foi alcançada
        int csock = accept(_socket, caddr, &caddrlen); // retorna um novo socket (client socket)
                                                    // dá accept no _socket e cria um outro socket para falar com o cliente (socket que recebe conexao e que conversa com o cliente)  
        char mss[BUFSZ];
        memset(mss, 0, BUFSZ);

        if(client_count > 10) {
            sprintf(mss, "%s(0%d)\n", ERROR, 1);
            printf("%s\n", mss);
            send(csock, mss, strlen(mss)+1, 0);
            close(csock);
        } else {
            send(csock, "ok", strlen("ok")+1, 0);

            struct client_data *cdata = malloc(sizeof(*cdata));
            
            cdata->csock = csock;
            memcpy(&(cdata->storage), &cstorage, sizeof(cstorage)); //copia a estrutura inteira de cstorage para cdata->storage 

            // se nao deu erro, deve-se criar uma thread 
            pthread_t tid;
            pthread_create(&tid, NULL, client_thread, cdata); // funcao que roda "em paralelo" e permite o servidor "voltar para o while", ou seja, sempre esta pronto para receber o novo cliente      

            client_count++;
            if (csock > fdmax) {
                fdmax = csock;
            }                                   
        }
    }
}