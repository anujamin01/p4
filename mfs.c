#include "mfs.h"
#include "udp.h"
#include "msg.h"
#include <string.h>

struct sockaddr_in server_addr;
//struct sockadd_in addrSnd;
struct sockadd_in addrRcv;
int fd;
int MFS_Init(char *hostname, int port){
    msg_t message;
    s_msg_t server_message;
    int fd;
    int rc = UDP_FillSockAddr(&server_addr, hostname, port);
    //int sd = UDP_Open(port);

    message.hostname = hostname;
    message.port = port;
    if(rc != 0){
        printf("Failed to set up server address\n");
        return rc;
    }
    fd = UDP_Open(7612);
    
    
    printf("client:: send message");
    rc = UDP_Write(fd, &server_addr, (char* )&message, 1000);
    if (rc < 0) {
        printf("client:: failed to send\n");
        exit(1);
    }

    printf("client:: wait for reply...\n");
    
    rc = UDP_Read(fd, &addrRcv, (char* )&server_message, 1000);
    printf("client:: got reply [size:%d contents:(%s)\n", server_message.returnCode, server_message.buffer);
    return 0;
}

int MFS_Lookup(int pinum, char *name){
    if (pinum < 0){
        return -1;
    }
    if(strlen(name) > 28 || name == NULL){
        return -1;
    }
    
    msg_t m;
    s_msg_t s_m;
    m.func = LOOKUP;
    m.pinum = pinum;
    memcpy(m.name, name, strlen(name));

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(m));

    UDP_Read(fd, &addrRcv, (char*)&s_m, sizeof(s_m));
    return s_m.returnCode;
}

int MFS_Stat(int inum, MFS_Stat_t *m){
    msg_t message;
    s_msg_t server_message;
    message.func = STAT;
    message.inum = inum;
    UDP_Write(fd, &server_addr, (char* )&message, sizeof(message));

    UDP_Read(fd, &addrRcv, (char*)&server_message, sizeof(server_message));
    //m->size = server_message.m.size;
    //m->type = server_message.m.type;
    memcpy(m, &server_message.m,sizeof(MFS_Stat_t));
    return server_message.returnCode;
    //return 0;
}

int MFS_Write(int inum, char *buffer, int offset, int nbytes){
    msg_t m;
    s_msg_t s_m;
    m.func = WRITE;
    m.inum = inum;
    memcpy(m.buffer, buffer, nbytes);
    m.offset = offset;
    m.nbytes = nbytes;

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(m));

    UDP_Read(fd, &addrRcv, (char*)&s_m, sizeof(s_m));
    return s_m.returnCode;
}

int MFS_Read(int inum, char *buffer, int offset, int nbytes){
    msg_t m;
    s_msg_t s_m;

    m.func = READ;
    m.inum = inum;
    //memcpy( m.buffer,buffer, nbytes);
    m.offset = offset;
    m.nbytes = nbytes;

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(m));

    UDP_Read(fd, &addrRcv, (char*)&s_m, sizeof(s_m)); 
    if(s_m.type == 0){
        // TODO: fill up a MFS_DirEnt_t struct?
        return 0;
    }
    memcpy(buffer, s_m.buffer, nbytes);
    return s_m.returnCode;
}

int MFS_Creat(int pinum, int type, char *name){
    msg_t m;
    s_msg_t s_m;

    m.func = CREAT;
    m.pinum = pinum;
    m.type = type;
    unsigned long name_size = sizeof(name);
    memcpy(m.name, name, name_size);

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(m));

    UDP_Read(fd, &addrRcv, (char*)&s_m, sizeof(s_m)); 

    return s_m.returnCode;
}

int MFS_Unlink(int pinum, char *name){
    msg_t m;
    s_msg_t s_m;

    m.func = UNLINK;
    m.pinum = pinum;
    unsigned long name_size = sizeof(name);
    memcpy(m.name,name,name_size);

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(m));

    UDP_Read(fd, &addrRcv, (char*)&s_m, sizeof(s_m)); 

    return s_m.returnCode;
}

int MFS_Shutdown(){
    msg_t m;
    s_msg_t s_m;

    m.func = SHUTDOWN;

    UDP_Write(fd, &server_addr, (char* )&m, sizeof(m));
    UDP_Read(fd, &addrRcv, (char*)&s_m, sizeof(s_m)); 

    return s_m.returnCode;
}
