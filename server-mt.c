#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define BUFSZ 500 //buff size

#define OK01 "OK(01)"
#define ERROR01 "ERROR(01)"
#define ERROR02 "ERROR(02)"
#define KILL "kill"
#define REQ_ADD "REQ_ADD"
#define RES_ADD "RES_ADD"
#define REQ_REM "REQ_REM"
#define REQ_INFOSE "REQ_INFOSE"
#define RES_INFOSE "RES_INFOSE"
#define REQ_INFOSCII "REQ_INFOSCII"
#define RES_INFOSCII "RES_INFOSCII"
#define REQ_STATUS "REQ_STATUS"
#define RES_STATUS "RES_STATUS"
#define REQ_UP "REQ_UP"
#define RES_UP "RES_UP"
#define REQ_NONE "REQ_NONE"
#define RES_NONE "RES_NONE"
#define REQ_DOWN "REQ_DOWN"
#define RES_DOWN "RES_DOWN"
#define SE_PORT 12345
#define SCII_PORT 54321
#define HIGH_PRODUCTION 41
#define MODERATE_PRODUCTION 31
#define LOW_PRODUCTION 20

#define SE_SERVER "SE"
#define SCII_SERVER "SCII"

int clients[10] = {0};

int production = -1;
int consumption = -1, old_consumption = -1;

void generateRandomProduction() {
    srand(time(NULL));
    int aux = rand() % 30 + 20;
    production = aux;
}

void generateRandomConsumption() {
    srand(time(NULL));
    int aux = rand() % 101;
    consumption = aux;
}

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

size_t parce_rcv_message(char *buf, struct client_data *cdata) {
    char mss[BUFSZ];
    memset(mss, 0, BUFSZ);

    //printf("msg recebida: %s\n", buf);
    int value1;

    int toClose = 0;

    if(strncmp(buf, REQ_ADD, strlen(REQ_ADD)+1) == 0) {
        int client_id = getClientID();

        if(client_id == -1) {
            sprintf(mss, "%s", ERROR01);
            printf("Client limit exceeded\n");
            toClose = 1;
        } else {
            sprintf(mss, "%s(%d)", RES_ADD, client_id);
            printf("Client %d added\n", client_id);
        }
    } else if(strncmp(buf, REQ_REM, strlen(REQ_REM)) == 0) {
        sscanf(buf, "REQ_REM(%d)", &value1);
        if(clients[value1-1] == 0) sprintf(mss, "%s", "ERROR(02)");
        else {
            clients[value1-1] = 0;
            sprintf(mss, "%s", "OK(01)");
            printf("Servidor Client %d removed\n", value1);
            toClose = 1;
        }
    } else if(strcmp(buf, REQ_INFOSE) == 0) sprintf(mss, "RES_INFOSE %d", production);
    else if(strcmp(buf, REQ_INFOSCII) == 0) sprintf(mss, "RES_INFOSCII %d", consumption);
    else if(strcmp(buf, REQ_STATUS) == 0) {
        if(production >= HIGH_PRODUCTION) sprintf(mss, "RES_STATUS %s", "alta");
        else if(production >= MODERATE_PRODUCTION && production < HIGH_PRODUCTION) sprintf(mss, "RES_STATUS %s", "moderada");
        else sprintf(mss, "RES_STATUS %s", "baixa");

        printf("baixa: x >= 20 <= 30\nmoderada: x >= 31 <= 40\nalta: x >= 41\natual: %d\n", production);

        generateRandomProduction();
    } else if(strcmp(buf, REQ_UP) == 0) {
        old_consumption = consumption;
        while(1) {
            generateRandomConsumption();
            if(consumption >= old_consumption) break;
        }
        sprintf(mss, "RES_UP %d %d", old_consumption, consumption);
    } else if(strcmp(buf, REQ_NONE) == 0) {
        sprintf(mss, "RES_NONE %d", consumption);
    }
    else if(strcmp(buf, REQ_DOWN) == 0) {
        old_consumption = consumption;
        while(1) {
            generateRandomConsumption();
            if(consumption <= old_consumption) break;
        }
        sprintf(mss, "RES_DOWN %d %d", old_consumption, consumption);
    }

    //printf("enviada pelo servidor: %s\n", mss);

    send(cdata->csock, mss, strlen(mss)+1, 0);

    return toClose;
}

void *client_thread(void *data) {
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);

    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);
    printf("[log] connection from %s\n", caddrstr); // printa sempre que o cliente conecta

    char buf[BUFSZ];

    while(1) {
        memset(buf, 0, BUFSZ);

        recv(cdata->csock, buf, BUFSZ-1, 0);

        size_t toClose = parce_rcv_message(buf, cdata);

        if(toClose == 1) break;
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
    if(setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &enable, sizeof(int)) != 0) logexit("setsockopt"); 

    //o servidor nao da connect, mas sim, bind() (depois o listen e depois o accept)

    //bind
    struct sockaddr *addr = (struct sockaddr *)(&storage);
    if(bind(_socket, addr, sizeof(storage)) != 0) logexit("bind");

    //listen
    if(listen(_socket, 10) != 0) logexit("listen"); // 10 -> quantidade de conexoes que podem estar pendentes para tratamento (pode ser outro valor)

    char addrstr[BUFSZ];
    addrtostr(addr, addrstr, BUFSZ);
    printf("bound to %s, waiting connections\n", addrstr);

    fd_set master;
    FD_ZERO(&master);
    FD_SET(_socket, &master);
    int fdmax = _socket;

    generateRandomProduction();
    generateRandomConsumption();

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

        struct client_data *cdata = malloc(sizeof(*cdata));
        
        cdata->csock = csock;
        memcpy(&(cdata->storage), &cstorage, sizeof(cstorage)); //copia a estrutura inteira de cstorage para cdata->storage 

        // se nao deu erro, deve-se criar uma thread 
        pthread_t tid;
        pthread_create(&tid, NULL, client_thread, cdata); // funcao que roda "em paralelo" e permite o servidor "voltar para o while", ou seja, sempre esta pronto para receber o novo cliente      
        
        if (csock > fdmax) {
            fdmax = csock;
        }                                   
    }
}