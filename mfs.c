#include "mfs.h"
#include "udp.h"
#include "msg.h"
#include <string.h>

int on = 0;

struct sockaddr_in server_addr;
int fd;

int MFS_Init(char *hostname, int port){
    /*
    struct sockaddr_in addrSnd, addrRcv;
    int sd = UDP_Open(20000);
    int rc = UDP_FillSockAddr(&addrSnd, "localhost", 10000);

    msg_t message;
    message.hostname = hostname;
    message.port = port;

    printf("client:: send message");
    rc = UDP_Write(sd, &addrSnd, (char* )&message, 1000);
    if (rc < 0) {
        printf("client:: failed to send\n");
        exit(1);
    }

    printf("client:: wait for reply...\n");
    s_msg_t server_message;
    rc = UDP_Read(sd, &addrRcv, (char* )&server_message, 1000);
    printf("client:: got reply [size:%d contents:(%s)\n", server_message.returnCode, server_message.buffer);
    return 0;
    */

    int rc = UDP_FillSockAddr(&server_addr, hostname, port);
    if(rc != 0){
        printf("Failed to set up server address\n");
        return rc;
    }
    fd = UDP_Open(7612);
    return 0;
}

int MFS_Lookup(int pinum, char *name){
    //printf("mfs 54\n");
    if (pinum < 0){
        //printf("in pinum if\n");
        return -1;
    }
    if(strlen(name) > 100 || name == NULL){
        //printf("in strlen name check if\n");
        return -1;
    }
    
    //printf("mfs 54\n");
    msg_t *m;
    s_msg_t *s_m;
    m->func = LOOKUP;
    m->pinum = pinum;
    //printf("mfs 58\n");
    memcpy(m->name, name, strlen(name));
    //printf("mfs 60\n");

    //printf("BEFORE WRITE\n");
    UDP_Write(fd, &server_addr, (char* )m, sizeof(m));
    //printf("PASSED WRITE\n");

    struct sockaddr_in ret_addr;
    UDP_Read(fd, &ret_addr, (char*)s_m, sizeof(s_m));
    //printf("in return code: %d, %d\n", s_m->inode, s_m->returnCode);
    return s_m->inode;
}

int MFS_Stat(int inum, MFS_Stat_t *m){
    msg_t message;
    s_msg_t server_message;
    message.func = STAT;
    message.inum = inum;
    
    UDP_Write(fd, &server_addr, (char* )&message, sizeof(message));

    struct sockaddr_in ret_addr;

    UDP_Read(fd, &ret_addr, (char*)&server_message, sizeof(server_message));
    memcpy(&server_message.m, m, sizeof(MFS_Stat_t));
    return server_message.returnCode;
    //return 0;
}

int MFS_Write(int inum, char *buffer, int offset, int nbytes){
    msg_t m;
    s_msg_t s_m;
    m.func = WRITE;
    m.inum = inum;
    memcpy(buffer, m.buffer, nbytes);
    m.offset = offset;
    m.nbytes = nbytes;

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(m));

    struct sockaddr_in ret_addr;
    UDP_Read(fd, &ret_addr, (char*)&s_m, sizeof(s_m));
    return 0;
}

int MFS_Read(int inum, char *buffer, int offset, int nbytes){
    msg_t m;
    s_msg_t s_m;

    m.func = READ;
    m.inum = inum;
    memcpy(buffer, m.buffer, nbytes);
    m.offset = offset;
    m.nbytes = nbytes;

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(m));

    struct sockaddr_in ret_addr;
    UDP_Read(fd, &ret_addr, (char*)&s_m, sizeof(s_m)); 
    if(s_m.type == 0){
        // TODO: fill up a MFS_DirEnt_t struct?
        return 0;
    }
    memcpy(buffer, s_m.buffer, nbytes);
    return 0;
}

int MFS_Creat(int pinum, int type, char *name){
    //printf("got to mfs_creat\n");
    msg_t *m = malloc(sizeof(msg_t));
    s_msg_t *s_m = malloc(sizeof(s_msg_t));

    m->func = CREAT;
    m->pinum = pinum;
    m->type = type;
    //unsigned long name_size = sizeof(name);
    strncpy(m->name, name, 28);
    //memcpy(m.name, m.name, name_size);

    UDP_Write(fd, &server_addr, (char* )m, sizeof(m));

    struct sockaddr_in ret_addr;
    UDP_Read(fd, &ret_addr, (char*)s_m, sizeof(s_m)); 

    return s_m->returnCode;
}

int MFS_Unlink(int pinum, char *name){
    msg_t m;
    s_msg_t s_m;

    m.func = UNLINK;
    m.pinum = pinum;
    unsigned long name_size = sizeof(name);
    memcpy(name, m.name, name_size);

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(m));

    struct sockaddr_in ret_addr;
    UDP_Read(fd, &ret_addr, (char*)&s_m, sizeof(s_m)); 

    return s_m.returnCode;
}

int MFS_Shutdown(){
    msg_t m;
    s_msg_t s_m;

    m.func = SHUTDOWN;

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(m));

    struct sockaddr_in ret_addr;
    UDP_Read(fd, &ret_addr, (char*)&s_m, sizeof(s_m)); 

    return s_m.returnCode;
}
