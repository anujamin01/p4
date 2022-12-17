#include "mfs.h"
#include "udp.h"
#include "msg.h"
#include <string.h>

int on = 0;

struct sockaddr_in server_addr;
int fd;

int MFS_Init(char *hostname, int port){
    int rc = UDP_FillSockAddr(&server_addr, hostname, port);
    if(rc != 0){
        printf("Failed to set up server address\n");
        return rc;
    }
    fd = UDP_Open(7612);
    return 0;
}

int MFS_Lookup(int pinum, char *name){
    if (pinum < 0){
        printf("pinum\n");
        return -1;
    }
    if(strlen(name) > 100 || name == NULL){
        printf("name\n");
    }
    
    msg_t m;
    s_msg_t s_m;
    m.func = LOOKUP;
    m.pinum = pinum;
    int i = 0;
    for (i = 0; i < strlen(name); i++)
    {
        m.name[i] = name[i];
    }
    m.name[i] = '\0';
    //memcpy(m.name, name, strlen(name));

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(m));

    struct sockaddr_in ret_addr;
    UDP_Read(fd, &ret_addr, (char*)&s_m, sizeof(s_m));
    return s_m.inode;
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
    /*
    printf("Reciving variables...");
    printf("pinum %d\n",pinum);
    printf("pinum %d\n",type);
    printf("pinum %s\n",name);
    */

    if (strlen(name) > 28 || name == NULL){
        return -1;
    }
    msg_t message;
    s_msg_t s_m;
    message.func = CREAT;
    message.pinum = pinum;
    message.type = type;
    int i = 0;
    for (i = 0; i < strlen(name); i++)
    {
        message.name[i] = name[i];
    }
    message.name[i] = '\0';

    /*
    printf("Sending variables...");
    printf("pinum %d\n",message.pinum);
    printf("pinum %d\n",message.type);
    printf("pinum %s\n",message.name);
    */

    UDP_Write(fd, &server_addr, (char* )&message, sizeof(message));

    struct sockaddr_in ret_addr;
    UDP_Read(fd, &ret_addr, (char*)&s_m, sizeof(s_m)); 

    return s_m.returnCode;
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
    UDP_Close(fd);
    return 0;
}
