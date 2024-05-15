#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 500 //buff size

#define ERROR01 "Client limit exceeded\n"
#define OK01 "Successful disconnect\n"
#define REQ_ADD "REQ_ADD"
#define RES_ADD "RES_ADD"

int client_count = 0; // Contador de clientes conectados
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER; // Mutex para controlar acesso ao contador

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

void *client_thread(void *data) {
    struct client_data *cdata = (struct client_data *)data;
    struct sockaddr *caddr = (struct sockaddr *)(&cdata->storage);

    char caddrstr[BUFSZ];
    addrtostr(caddr, caddrstr, BUFSZ);
    printf("[log] connection from %s\n", caddrstr); // printa sempre que o cliente conecta

    //leitura da msg do cliente
    char buf[BUFSZ];
    memset(buf, 0, BUFSZ);

    client_count++;
    size_t count = recv(cdata->csock, buf, BUFSZ-1, 0); // retorna numero de bytes recebido
    if (strcmp(buf, REQ_ADD) == 0) {
        // Gera um identificador único para o cliente
        int client_id = client_count-1;

        // Envia a mensagem de resposta com o identificador
        sprintf(buf, "%s(%d)", RES_ADD, client_id);
        send(cdata->csock, buf, strlen(buf) + 1, 0);

        printf("Client %d added\n", client_id);
    } else printf("outra msg");

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

    // tratamento das conexoes pelo while
    while(1) {
        struct sockaddr_storage cstorage; //client storage
        struct sockaddr *caddr = (struct sockaddr *)(&cstorage);
        socklen_t caddrlen = sizeof(cstorage); 

            // Verifica se a quantidade máxima de conexões foi alcançada
        int csock = accept(_socket, caddr, &caddrlen); // retorna um novo socket (client socket)
                                                    // dá accept no _socket e cria um outro socket para falar com o cliente (socket que recebe conexao e que conversa com o cliente)  
        
        if (client_count > 10) {
            printf("%s\n", ERROR01);
            send(csock, ERROR01, strlen(ERROR01)+1, 0);
            pthread_exit(NULL);
        } else {
            send(csock, "ok", strlen("ok")+1, 0);

            struct client_data *cdata = malloc(sizeof(*cdata));
            
            cdata->csock = csock;
            memcpy(&(cdata->storage), &cstorage, sizeof(cstorage)); //copia a estrutura inteira de cstorage para cdata->storage 

            // se nao deu erro, deve-se criar uma thread 
            pthread_t tid;
            pthread_create(&tid, NULL, client_thread, cdata); // funcao que roda "em paralelo" e permite o servidor "voltar para o while", ou seja, sempre esta pronto para receber o novo cliente                                           
        }

    }

    pthread_mutex_destroy(&count_mutex);
}