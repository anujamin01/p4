
#include <stdio.h>
#include <stdlib.h>
#include "mfs.h"
#include "ufs.h"
#include "udp.h"
#define BUFFER_SIZE (1000)

// client code
int main(int argc, char *argv[]) {
    /*
    char *hostname = argv[1];
    int port = atoi(argv[2]);
    char* cmd = argv[3];
    int rc = MFS_Init(hostname,port);

    if (rc != 0){
        return -1;
    }
    // will need to check return codes for the client
    if (strcmp(cmd,"stat") == 0){
        int inum = atoi(argv[4]);
        MFS_Stat_t stat;
        int ret = MFS_Stat(inum, &stat);
        if (ret != 0){
            printf("Error\n");
        } else{
            printf("File Type: %d\n", stat.type);
            printf("File Size: %d\n", stat.size);
        }
    }
    */
    struct sockaddr_in addrSnd, addrRcv;
    
    int sd = UDP_Open(20000);
    int rc = UDP_FillSockAddr(&addrSnd, "localhost", 10000);

    // "function~hostname~port~pinum~name~*m~buffer~offset~nbytes~type"
    // "0~a~1~1~a~1~a~1~1~1"

    char message[BUFFER_SIZE];

    printf("client:: send message [%s]\n", message);
    rc = UDP_Write(sd, &addrSnd, message, BUFFER_SIZE); // change message to encoded_msg?
    if (rc < 0) {
	printf("client:: failed to send\n");
	exit(1);
    }

    printf("client:: wait for reply...\n");
    rc = UDP_Read(sd, &addrRcv, message, BUFFER_SIZE);
    printf("client:: got reply [size:%d contents:(%s)\n", rc, message);
    return 0;
}
