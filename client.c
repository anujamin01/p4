#include <mfs.h>

int MFS_Init(char *hostname, int port);
int MFS_Lookup(int pinum, char *name);
int MFS_Stat(int inum, MFS_Stat_t *m);
int MFS_Write(int inum, char *buffer, int offset, int nbytes);
int MFS_Read(int inum, char *buffer, int offset, int nbytes);
int MFS_Creat(int pinum, int type, char *name);
int MFS_Unlink(int pinum, char *name);
int MFS_Shutdown();

/*
#include <stdio.h>
#include "udp.h"

#define BUFFER_SIZE (1000)

// client code
int main(int argc, char *argv[]) {
    struct sockaddr_in addrSnd, addrRcv;

    int sd = UDP_Open(20000);
    int rc = UDP_FillSockAddr(&addrSnd, "localhost", 10000);

    char message[BUFFER_SIZE];
    sprintf(message, "hello world");

    printf("client:: send message [%s]\n", message);
    rc = UDP_Write(sd, &addrSnd, message, BUFFER_SIZE);
    if (rc < 0) {
	printf("client:: failed to send\n");
	exit(1);
    }

    printf("client:: wait for reply...\n");
    rc = UDP_Read(sd, &addrRcv, message, BUFFER_SIZE);
    printf("client:: got reply [size:%d contents:(%s)\n", rc, message);
    return 0;
}
*/