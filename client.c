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

int initialConnection(int _socket, int sid) {
    char msg[BUFSZ];
    size_t count;
    int cid;
    count = send(_socket, REQ_ADD, strlen(REQ_ADD)+1, 0);

    if(count != 0) {
        recv(_socket, msg, BUFSZ, 0);

        sscanf(msg, "RES_ADD(%d)", &cid);

        if(sid == 0) printf("Servidor SE new ID: %d\n", cid);
        if(sid == 1) printf("Servidor SCII new ID: %d\n", cid);
    }

    return cid;
}

void handleComunication(int _socket, char* mss, int id) {
    size_t count;
    count = send(_socket, mss, strlen(mss)+1, 0);
    printf("id: %d\n", id);
    char res[BUFSZ];
    memset(res, 0, BUFSZ);
    if(count != 0) {
        recv(_socket, res, BUFSZ, 0);
        printf("return: %s\n", res);
    }
}

int main(int argc, char **argv) {
    if (argc < 4) usage(argc, argv);

    // Connect to first server
    struct sockaddr_storage storage1;
    if(addrparse(argv[1], argv[2], &storage1) != 0) usage(argc, argv);

    int _socket1;
    _socket1 = socket(storage1.ss_family, SOCK_STREAM, 0);
    if(_socket1 == -1) logexit("socket");

    struct sockaddr *addr1 = (struct sockaddr *)(&storage1);

    if(connect(_socket1, addr1, sizeof(storage1)) != 0) logexit("connect");

    // Connect to second server
    struct sockaddr_storage storage2;
    if(addrparse(argv[1], argv[3], &storage2) != 0) usage(argc, argv);

    int _socket2;
    _socket2 = socket(storage2.ss_family, SOCK_STREAM, 0);
    if(_socket2 == -1) logexit("socket");

    struct sockaddr *addr2 = (struct sockaddr *)(&storage2);

    if(connect(_socket2, addr2, sizeof(storage2)) != 0) logexit("connect");

    char buf[BUFSZ];
    char mss[BUFSZ];

    recv(_socket1, buf, BUFSZ, 0);

    if(strncmp(buf, ERROR01, strlen(ERROR01)+1) == 0) {
        printf("%s", ERROR01);
    } else {
        char addrstr1[BUFSZ];
        char addrstr2[BUFSZ];
        addrtostr(addr1, addrstr1, BUFSZ);
        addrtostr(addr2, addrstr2, BUFSZ);
        printf("conntected to %s\n", addrstr1);
        printf("conntected to %s\n", addrstr2);

        int cid;

        cid = initialConnection(_socket1, 0);
        cid = initialConnection(_socket2, 1);

        while(1) {
            memset(buf, 0, BUFSZ); //inicializa o buffer como 0
            memset(mss, 0, BUFSZ);

            printf("mensagem: ");
            fgets(buf, BUFSZ-1, stdin); //le do teclado o que o user vai digitar

            int msgID = -1; // identificador para indicar para qual servidor a msg deve ser enviada

            if(strncmp(buf, "kill", strlen("kill")) == 0) {
                sprintf(mss, "REQ_REM(%d)", cid);
                msgID = 2;
            } else {
                printf("outra msg\n");
                break;
            }

            printf("msg enviada: %s\n", mss);

            if(msgID == 0 || msgID == 2) handleComunication(_socket1, mss, 1);
            if(msgID == 1 || msgID == 2) handleComunication(_socket2, mss, 2);

            memset(mss, 0, BUFSZ); //inicializa o buffer como 0
        }
    }

    close(_socket1);
    close(_socket2);
}