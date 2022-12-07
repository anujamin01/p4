#include "mfs.h"
#include "udp.h"
#include "msg.h"

int online = 0;

struct sockaddr_in server_addr;
int fd;

int MFS_Init(char *hostname, int port){
    int rc = UDP_FillSockAddr(&server_addr, hostname, port);
    if(rc != 0){
        printf("Failed to set up server address\m");
        return rc;
    }
    fd = UDP_Open(port);
    return 0;
}

int MFS_Lookup(int pinum, char *name){
    if(!online){
        return -1;
    }
    if (pinum < 0){
        return -1;
    }
    if(strlen(name) > 100 || name == NULL){
        return -1;
    }
    
    msg_t m;
    m.func = LOOKUP;
    m.pinum = pinum;
    m.name = name;

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(msg_t));
}

int MFS_Stat(int inum, MFS_Stat_t *m){
    msg_t message;
    message.func = STAT;
    message.type = m;
    message.inum = inum;
    
    UDP_Write(fd, &server_addr, (char* )&message, sizeof(msg_t));

    // TODO: probably something here

    return 0;
}

int MFS_Write(int inum, char *buffer, int offset, int nbytes){
    msg_t m;
    m.func = WRITE;
    m.inum = inum;
    m.buffer = buffer;
    m.offset = offset;
    m.nbytes = nbytes;

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(msg_t));
}

int MFS_Read(int inum, char *buffer, int offset, int nbytes){
    msg_t m;
    m.func = READ;
    m.inum = inum;
    m.buffer = buffer;
    m.offset = offset;
    m.nbytes = nbytes;

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(msg_t));
}

int MFS_Creat(int pinum, int type, char *name){
    msg_t m;
    m.func = CREAT;
    m.pinum = pinum;
    m.type = type;
    m.name = name;

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(msg_t));
}

int MFS_Unlink(int pinum, char *name){
    msg_t m;
    m.func = UNLINK;
    m.pinum = pinum;
    m.name = name;

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(msg_t));
}

int MFS_Shutdown(){
    exit(0);
}
