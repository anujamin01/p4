#include <stdio.h>
#include "udp.h"
#include "msg.h"
#include "mfs.h"

#define BUFFER_SIZE (1000)

// client code
int main(int argc, char *argv[]) {
    MFS_Init("localhost", atoi(argv[1]));

    printf("BEFORE CREAT\n");
    printf("Creating blank dir: %d\n",MFS_Creat(0,0,"dir1"));
    //printf("Creating blank dir: %d\n",MFS_Creat(0,1,"file1"));
    printf("AFTER CREAT\n");

    printf("BEFORE LOOKUP\n");
    printf("Lookup returned: %d\n", MFS_Lookup(0, "dir1"));
    printf("AFTER LOOKUP\n");

    /*
    printf("got past init\n");
    printf("Creating blank dir: %d\n",MFS_Creat(0,0,"dir1"));
    printf("got past creat\n");
    printf("Looking up blank dir: %d\n",MFS_Lookup(0,"dir1"));
    */

    printf("Shtdown: %d\n", MFS_Shutdown());
}

/*
Make foo.c
Include the shared libraries
Replicate all the commands that the tester does
For server.c
Compile it with -g flag so u can use gdb
Once you open gdb type tui enable
*/
