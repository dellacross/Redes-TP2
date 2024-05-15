#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFSZ 500 //buff size

#define ERROR01 "Client limit exceeded"
#define OK01 "Successful disconnect"
#define REQ_ADD "REQ_ADD"
#define RES_ADD "RES_ADD"

void usage(int argc, char **argv) {
    printf("usage: %s <server IP> <server port>\n", argv[0]);
    printf("example: %s 127.0.0.1 51551\n", argv[0]);
    exit(EXIT_FAILURE);
}

int valid_class_identifier(char *sala_id_s) {
    int sala_id = atoi(sala_id_s);
    return sala_id >= 0 && sala_id <= 7;
}

int main(int argc, char **argv) {
    if (argc < 4) usage(argc, argv);

    // o connect recebe um ponteiro para a struct sockaddr
    struct sockaddr_storage storage;
    if(addrparse(argv[1], argv[2], &storage) != 0) usage(argc, argv); // argv[1] -> endereco do servidor | argv[2] -> porto que recebeu | ponteiro para o storage 

    int _socket;
    _socket = socket(storage.ss_family, SOCK_STREAM, 0); //storage.ss_family -> tipo do protocolo (ipv4 | ipv6) | SOCK_STREAM -> SOCKET TCP
    if(_socket == -1) logexit("socket");

    struct sockaddr *addr = (struct sockaddr *)(&storage); //pega o ponteiro para o storage, converte para o tipo do ponteiro *addr (sockaddr) e joga para a variavel addr, para depois passar para o connect

    if(connect(_socket, addr, sizeof(storage)) != 0) logexit("connect"); //addr -> endereco do servidor

    size_t count;
    char buf[BUFSZ];
    char mss[BUFSZ];

    count = recv(_socket, buf, BUFSZ, 0);

    printf("%ld - %s\n", count, buf);

    if(strncmp(buf, ERROR01, strlen(ERROR01)+1) == 0) {
        printf("%s", ERROR01);
    } else {
        char addrstr[BUFSZ];
        addrtostr(addr, addrstr, BUFSZ);

        printf("conntected to %s\n", addrstr);

        count = send(_socket, REQ_ADD, strlen(REQ_ADD)+1, 0);

        if(count != 0) {
            recv(_socket, buf, BUFSZ, 0);

            int id;

            sscanf(buf, "RES_ADD(%d)", &id);

            printf("Servidor new ID: %d\n", id);
        }

        while(1) {
            memset(buf, 0, BUFSZ); //inicializa o buffer como 0
            memset(mss, 0, BUFSZ);

            printf("mensagem: ");
            fgets(buf, BUFSZ-1, stdin); //le do teclado o que o user vai digitar

            // count -> nmr de bytes efetivamente transmitidos na rede
            if(count != (strlen(mss)+1)) logexit("exit"); // se o nmr de bytes for diferente do que foi pedido para se transmitir (strlen(buf)+1)

            // cliente recebe uma resposta (linha 51 a 62)
            memset(buf, 0, BUFSZ); //inicializa o buffer como 0

            if(count != 0) {
                recv(_socket, buf, BUFSZ, 0);
                printf("%s", buf);
            }
        }
    }

    close(_socket);
}