
#include <stdio.h>
#include <stdlib.h>
#include "mfs.h"
#include "ufs.h"
#include "udp.h"
#define BUFFER_SIZE (1000)

// client code
int main(int argc, char *argv[]) {
    struct sockaddr_in addrSnd, addrRcv;

    int sd = UDP_Open(20000);
    int rc = UDP_FillSockAddr(&addrSnd, "localhost", 10000);

    char message[BUFFER_SIZE]; // messages like: "function~hostname~port~pinum~name~*m~buffer~offset~nbytes~type"
    msg_t encoded_msg;

    printf("client:: send message [%s]\n", message);
    rc = UDP_Write(sd, &addrSnd, &message, BUFFER_SIZE); // change message to encoded_msg?
    if (rc < 0) {
	printf("client:: failed to send\n");
	exit(1);
    }

    printf("client:: wait for reply...\n");
    rc = UDP_Read(sd, &addrRcv, message, BUFFER_SIZE);
    printf("client:: got reply [size:%d contents:(%s)\n", rc, message);
    return 0;
}
