#include <stdio.h>
#include "udp.h"
#include "msg.h"
#include "mfs.h"

#define BUFFER_SIZE (1000)

// client code
int main(int argc, char *argv[]) {
    MFS_Init("HOSTNAME", 10000);

    printf("Creating blank dir: %d\n",MFS_Creat(1,0,'dir1'));
    printf("Looking up blank dir: %d\n",MFS_Lookup(0,'dir1'));

    MFS_Shutdown();
}

/*
Make foo.c
Include the shared libraries
Replicate all the commands that the tester does
For server.c
Compile it with -g flag so u can use gdb
Once you open gdb type tui enable
*/
