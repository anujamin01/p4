#include "mfs.h"
#include "udp.h"
#include "msg.h"
#include "ufs.h"
#include <string.h>
#include <time.h>
#include <stdio.h>

int on = 0;

struct sockaddr_in server_addr, ret_addr;
int fd;

int MFS_Init(char *hostname, int port){
    // open random port
    fd = UDP_Open(rand() % (10000) + 40000);
    if(fd < 0){
        return fd;
    }
    
    int rc = UDP_FillSockAddr(&server_addr, hostname, port);
    if(rc < 0){
        return rc;
    }
    return 0;
}

int MFS_Lookup(int pinum, char *name){
    if(pinum != 0 && strcmp(name, ".") == 0){
        return 1;
    }
    if(pinum != 0 && strcmp(name, "..") == 0){
        return 0;
    }
    if(fd<0){
        return fd;
    }
    if (pinum < 0){
        return -1;
    }
    if(strlen(name) > 28 || name == NULL){
        return -1;
    }
    
    msg_t m;

    m.func = LOOKUP;
    m.pinum = pinum;
    int i = 0;
    for (i = 0; i < strlen(name); i++)
    {
        m.name[i] = name[i];
    }
    m.name[i] = '\0';

    int rc = UDP_Write(fd, &server_addr, (char* )&m, sizeof(msg_t));

    rc = UDP_Read(fd, &ret_addr, (char*)&m, sizeof(msg_t));
    if(rc<0){
        return -1;
    }
    return m.returnCode;
}

int MFS_Stat(int inum, MFS_Stat_t *m){
    if(fd<0){
        return fd;
    }
    msg_t message;
    message.func = STAT;
    message.inum = inum;
    
    int rc = UDP_Write(fd, &server_addr, (char* )&message, sizeof(msg_t));

    rc = UDP_Read(fd, &ret_addr, (char*)&message, sizeof(msg_t));
    if(rc<0){
        return -1;
    }
    m->size = message.size;
    m->type = message.type;
    return message.returnCode;
}

int MFS_Write(int inum, char *buffer, int offset, int nbytes){
    if(fd<0){
        return fd;
    }
    msg_t m;
    m.func = WRITE;
    m.inum = inum;
    int i;
    for (i = 0; i < strlen(buffer); i++)
    {
        m.buffer[i] = buffer[i];
    }
    m.buffer[i] = '\0';
    m.offset = offset;
    m.nbytes = nbytes;

    int rc = UDP_Write(fd, &server_addr, (char* )&m, sizeof(msg_t));

    rc = UDP_Read(fd, &ret_addr, (char*)&m, sizeof(msg_t));
    if(rc < 0){
        return -1;
    }
    return m.returnCode;
}

int MFS_Read(int inum, char *buffer, int offset, int nbytes){
    if(fd<0){
        return fd;
    }
    if(nbytes>UFS_BLOCK_SIZE){
        return -1;
    }
    msg_t m;

    m.func = READ;
    m.inum = inum;
    int i;
    for (i = 0; i < strlen(buffer); i++)
    {
        m.buffer[i] = buffer[i];
    }
    m.buffer[i] = '\0';
    m.offset = offset;
    m.nbytes = nbytes;

    int rc = UDP_Write(fd, &server_addr, (char* )&m, sizeof(msg_t));

    rc = UDP_Read(fd, &ret_addr, (char*)&m, sizeof(msg_t)); 
    if(rc<0){
        return -1;
    }
    if(m.returnCode == -1){
        // TODO: fill up a MFS_DirEnt_t struct?
        return -1;
    }
    int j;
    for (j = 0; j < strlen(buffer); j++)
    {
        m.buffer[j] = buffer[j];
    }
    m.buffer[j] = '\0';
    return m.returnCode;
}

int MFS_Creat(int pinum, int type, char *name){
    if(fd<0){
        return fd;
    }

    if (strlen(name) > 28 || name == NULL){
        return -1;
    }
    
    msg_t message;
    message.func = CREAT;
    message.pinum = pinum;
    message.type = type;
    int i = 0;
    for (i = 0; i < strlen(name); i++)
    {
        message.name[i] = name[i];
    }
    message.name[i] = '\0';

    int rc = UDP_Write(fd, &server_addr, (char* )&message, sizeof(msg_t));

    rc = UDP_Read(fd, &ret_addr, (char*)&message, sizeof(msg_t)); 
    if(rc<0){
        return -1;
    }
    return message.returnCode;
}

int MFS_Unlink(int pinum, char *name){
    if(fd<0){
        return fd;
    }
    msg_t m;

    m.func = UNLINK;
    m.pinum = pinum;
    int i;
    for (i = 0; i < strlen(name); i++)
    {
        m.name[i] = name[i];
    }
    m.name[i] = '\0';

    int rc = UDP_Write(fd, &server_addr, (char* )&m, sizeof(msg_t));

    rc = UDP_Read(fd, &ret_addr, (char*)&m, sizeof(msg_t)); 
    if(rc<0){
        return -1;
    }

    return m.returnCode;
}

int MFS_Shutdown(){
   if(fd<0){
        return fd;
    }
    msg_t m;

    m.func = SHUTDOWN;

    int rc = UDP_Write(fd, &server_addr, (char* )&m, sizeof(msg_t));
    if(rc<0){
        return -1;
    }
    return 0;
}
