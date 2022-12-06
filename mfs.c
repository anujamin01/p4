#include <mfs.h>

char* host_name = NULL;
int server_port = 0;
int online = 0;

int MFS_Init(char *hostname, int port){
    host_name = strdup(hostname);
    server_port = port;
    if (server_port < 0 || host_name == NULL){
        return -1;
    }
    online = 1;
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

    // look up the entry "name" in it
    UDP_Packet tx;
}

int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int offset, int nbytes);
int MFS_Read(int inum, char *buffer, int offset, int nbytes);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);

int MFS_Shutdown(){
    exit(0);
}
