#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define BUFSZ 500 // buffer size

#define ERROR01 "ERROR(01)"
#define ERROR02 "ERROR(02)"
#define OK01 "OK(01)"
#define KILL "kill"
#define REQ_ADD "REQ_ADD"
#define RES_ADD "RES_ADD"
#define RES_REM "REQ_REM"
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
#define HIGH_PRODUCTION 41
#define MODERATE_PRODUCTION 31
#define LOW_PRODUCTION 20

void usage(int argc, char **argv) {
    printf("usage: %s <server IP> <SE port> <SCII port>\n", argv[0]);
    printf("example: %s 127.0.0.1 12345 54321\n", argv[0]);
    printf("argc: %d\n", argc);
    exit(EXIT_FAILURE);
}

int initialConnection(int _socketSE, int _socketSCII) {
    char buf[BUFSZ];
    memset(buf, 0, BUFSZ);
    int cid = -1;

    send(_socketSE, REQ_ADD, strlen(REQ_ADD) + 1, 0);
    recv(_socketSE, buf, BUFSZ, 0);

    if(strncmp(buf, ERROR01, strlen(ERROR01)) == 0) {
        printf("Client limit exceeded\n");
        return -1;
    } else {
        sscanf(buf, "RES_ADD(%d)", &cid);
        printf("Servidor new ID: %d\n", cid);
    }

    memset(buf, 0, BUFSZ);

    send(_socketSCII, REQ_ADD, strlen(REQ_ADD) + 1, 0);
    recv(_socketSCII, buf, BUFSZ, 0);

    if(strncmp(buf, ERROR01, strlen(ERROR01)) == 0) {
        printf("Client limit exceeded\n");
        return -1;
    } else printf("Servidor new ID: %d\n", cid);

    return cid;
}

int parse_send_message(int _socketSE, int _socketSCII, char* buf, int cid);

int parse_rcv_message(char* buf, int _socketSCII, int cid) {
    char mss[BUFSZ];
    memset(mss, 0, BUFSZ);

    int value1, value2;
    char word1[BUFSZ];

    int toClose = 0;

    //printf("rcv_msg: %s\n", buf);

    if(strncmp(buf, RES_INFOSE, strlen(RES_INFOSE)) == 0) {
        sscanf(buf, "RES_INFOSE %d", &value1);
        sprintf(mss, "Producao atual: %d", value1);
        printf("%s\n", mss);
    } else if(strncmp(buf, RES_INFOSCII, strlen(RES_INFOSCII)) == 0) {
        sscanf(buf, "RES_INFOSCII %d", &value1);
        sprintf(mss, "Consumo atual: %d", value1);
        printf("%s\n", mss);
    } else if(strncmp(buf, RES_STATUS, strlen(RES_STATUS)) == 0) {
        sscanf(buf, "RES_STATUS %s", word1);

        if(strncmp(word1, "alta", strlen("alta")) == 0) sprintf(mss, "%s", REQ_UP);
        else if(strncmp(word1, "moderada", strlen("moderada")) == 0) sprintf(mss, "%s", REQ_NONE);
        else if(strncmp(word1, "baixa", strlen("baixa")) == 0) sprintf(mss, "%s", REQ_DOWN);

        parse_send_message(0, _socketSCII, mss, cid);
    } else if(strncmp(buf, RES_UP, strlen(RES_UP)) == 0) {
        sscanf(buf, "RES_UP %d %d", &value1, &value2);
        sprintf(mss, "Consumo antigo: %d", value1);
        printf("%s\n", mss);
        memset(mss, 0, BUFSZ);
        sprintf(mss, "Consumo atual: %d", value2);
        printf("%s\n", mss);
    } else if(strncmp(buf, RES_NONE, strlen(RES_NONE)) == 0) {
        sscanf(buf, "RES_NONE %d", &value1);
        sprintf(mss, "Consumo antigo: %d", value1);
        printf("%s\n", mss);
    } else if(strncmp(buf, RES_DOWN, strlen(RES_DOWN)) == 0) {
        sscanf(buf, "RES_DOWN %d %d", &value1, &value2);
        sprintf(mss, "Consumo antigo: %d", value1);
        printf("%s\n", mss);
        memset(mss, 0, BUFSZ);
        sprintf(mss, "Consumo atual: %d", value2);
        printf("%s\n", mss);
    } else if(strncmp(buf, OK01, strlen(OK01)) == 0) {
        toClose = 1;
        printf("Servidor Client %d removed\n", cid);
    } else if(strncmp(buf, ERROR01, strlen(ERROR01)) == 0) {
        toClose = 1;
        printf("Client limit exceeded\n");
    } else if(strncmp(buf, ERROR02, strlen(ERROR02)) == 0) {
        toClose = 1;
        printf("Client not found\n");
    }

    return toClose;
}

int parse_send_message(int _socketSE, int _socketSCII, char* buf, int cid) {
    char mss[BUFSZ];
    memset(mss, 0, BUFSZ);
    int sid = -1;

    int toClose = 0;

    //printf("send buf: %s\n", buf);

    if(strncmp(buf, KILL, strlen(KILL)) == 0) {
        sprintf(mss, "REQ_REM(%d)", cid);
        sid = 2;
    }
    else if(strncmp(buf, "display info se", strlen("display info se")) == 0) {
        sprintf(mss, "%s", REQ_INFOSE);
        sid = 0;
    }
    else if(strncmp(buf, "display info scii", strlen("display info scii")) == 0) {
        sprintf(mss, "%s", REQ_INFOSCII);
        sid = 1;
    }
    else if(strncmp(buf, "query condition", strlen("query condition")) == 0) {
        sprintf(mss, "%s", REQ_STATUS);
        sid = 0;
    }
    else if(strncmp(buf, REQ_UP, strlen(REQ_UP)) == 0) {
        sprintf(mss, "%s", REQ_UP);
        sid = 1;
    }
    else if(strncmp(buf, REQ_NONE, strlen(REQ_NONE)) == 0){ 
        sprintf(mss, "%s", REQ_NONE);
        sid = 1;
    }
    else if(strncmp(buf, REQ_DOWN, strlen(REQ_DOWN)) == 0) {
        sprintf(mss, "%s", REQ_DOWN);
        sid = 1;
    }

    //printf("mss: %s - %d\n", mss, sid);

    char rcv_mss[BUFSZ];
    memset(rcv_mss, 0, BUFSZ);
    
    if(sid == 0 || sid == 2) {
        send(_socketSE, mss, strlen(mss) + 1, 0);
        recv(_socketSE, rcv_mss, BUFSZ, 0);
        toClose = parse_rcv_message(rcv_mss, _socketSCII, cid);
    }

    memset(rcv_mss, 0, BUFSZ);

    if(sid == 1 || sid == 2) {
        send(_socketSCII, mss, strlen(mss) + 1, 0);
        recv(_socketSCII, rcv_mss, BUFSZ, 0);
        toClose = parse_rcv_message(rcv_mss, _socketSCII, cid);
    } 

    return toClose;
}

int main(int argc, char **argv) {
    if (argc < 3) usage(argc, argv);

    // Connect to SE server
    struct sockaddr_storage storage_SE, storage_SCII;
    if (addrparse(argv[1], argv[2], &storage_SE) != 0) usage(argc, argv);
    int socket_SE = socket(storage_SE.ss_family, SOCK_STREAM, 0);
    struct sockaddr *addr_SE = (struct sockaddr *)(&storage_SE);
    if (connect(socket_SE, addr_SE, sizeof(storage_SE)) != 0) logexit("connect");

    // Connect to SCII server
    if (addrparse(argv[1], argv[3], &storage_SCII) != 0) usage(argc, argv);
    int socket_SCII = socket(storage_SCII.ss_family, SOCK_STREAM, 0);
    struct sockaddr *addr_SCII = (struct sockaddr *)(&storage_SCII);
    if (connect(socket_SCII, addr_SCII, sizeof(storage_SCII)) != 0) logexit("connect");

    //fd_set read_fds;
    int cid = initialConnection(socket_SE, socket_SCII);

    int toClose = 0;

    while (cid != -1) {
        /*
        FD_ZERO(&read_fds);
        FD_SET(socket_SE, &read_fds);
        FD_SET(socket_SCII, &read_fds);
        int max_fd = socket_SE > socket_SCII ? socket_SE : socket_SCII;

        select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        */

        char buf[BUFSZ];
        memset(buf, 0, BUFSZ);

        printf("mensagem: ");
        fgets(buf, BUFSZ - 1, stdin);
        
        /*

        if (FD_ISSET(socket_SE, &read_fds)) {
            count = parse_send_message(socket_SE, buf, cid);
            printf("SE\n");
            if(count != 0) parse_rcv_message(socket_SE, cid);
        }
        if (FD_ISSET(socket_SCII, &read_fds)) {
            count = parse_send_message(socket_SCII, buf, cid);
            printf("SCII\n");
            if(count != 0) parse_rcv_message(socket_SCII, cid);
        }
        */

        toClose = parse_send_message(socket_SE, socket_SCII, buf, cid);

        if(toClose == 1) break;
    }
}