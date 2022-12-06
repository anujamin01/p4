#include <stdio.h>
#include "udp.h"
#include "ufs.h"
#include "mfs.h"
#define BUFFER_SIZE (1000)

int fd = -1;

int Init(char *hostname, int port){
    return 0;
}
int Lookup(int pinum, char *name){
    return 0;
}
int Stat(int inum, MFS_Stat_t *m){
    return 0;
}
int Write(int inum, char *buffer, int offset, int nbytes){
    return 0;
}
int Read(int inum, char *buffer, int offset, int nbytes){
    return 0;
}
int Creat(int pinum, int type, char *name){
    return 0;
}
int Unlink(int pinum, char *name){
    return 0;
}
int Shutdown(){
    fsync(fd);
    exit(0);
}

// server code
int main(int argc, char *argv[]) {
    int sd = UDP_Open(10000);
    assert(sd > -1);
    while (1) {
	struct sockaddr_in addr;
	char message[BUFFER_SIZE];
    msg_t encoded_msg;
	printf("server:: waiting...\n");
	int rc = UDP_Read(sd, &addr, message, BUFFER_SIZE);

    if (rc < 0){
        return -1;
    }

    encoded_msg.func = atoi(strtok(message, "~"));
    encoded_msg.hostname= strtok(NULL, "~");
    encoded_msg.port = atoi(strtok(NULL, "~"));
    encoded_msg.pinum = atoi(strtok(NULL, "~"));
    encoded_msg.name = strtok(NULL, "~");
    encoded_msg.inum = atoi(strtok(NULL, "~"));
    encoded_msg.m = atoi(strtok(NULL, "~"));
    encoded_msg.buffer = strtok(NULL, "~");
    encoded_msg.offset = atoi(strtok(NULL, "~"));
    encoded_msg.nbytes = atoi(strtok(NULL, "~"));
    encoded_msg.type = atoi(strtok(NULL, "~"));


    int int_reply = NULL;
    switch(encoded_msg.func){
        case 0:
            int_reply = Init(encoded_msg.hostname, encoded_msg.port);
            break;
        case 1:
            int_reply = Lookup(encoded_msg.pinum, encoded_msg.name);
            break;
        case 2:
            int_reply = Stat(encoded_msg.inum, encoded_msg.m);
            break;
        
        case 3:
            int_reply = Write(encoded_msg.inum, encoded_msg.buffer, encoded_msg.offset, encoded_msg.nbytes);
            break;
        case 4:
            int_reply = Read(encoded_msg.inum, encoded_msg.buffer, encoded_msg.offset, encoded_msg.nbytes);
            break;
        case 5:
            int_reply = Creat(encoded_msg.pinum, encoded_msg.type, encoded_msg.name);
            break;
        case 6:
            int_reply = Unlink(encoded_msg.pinum, encoded_msg.name);
            break;
        case 7:
            int_reply = Shutdown();
            break;
    }

	printf("server:: read message [size:%d contents:(%s)]\n", rc, message);
	if (rc > 0) {
            char reply[BUFFER_SIZE];
            sprintf(reply, "goodbye world");
            rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
	    printf("server:: reply\n");
	} 
    }
    return 0; 
}
