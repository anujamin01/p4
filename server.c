#include <stdio.h>
#include "udp.h"
#include "ufs.h"
#include "mfs.h"
#include "msg.h"

#define BUFFER_SIZE (1000)

int fd = -1;

// TODO: fsync() after making some change to the file system
// so probably fsync() at the end of each method?

int Init(char *hostname, int port, struct sockaddr_in addr){
    // actually, probably don't need Init() in server

    /*
    // load image and send it to client?
    char message[BUFFER_SIZE];
    UDP_Write(fd, &addr, message, sizeof(BUFFER_SIZE));
    */

    return 0;
}
int Lookup(int pinum, char *name){
    // TODO:
    // from image file
    // from ufs.h, figure out where in the super_t struct we want to look. 
    // We will get a dir_ent_t if valid.
    // use the inum to get the inode_t
    // the first element of unsigned int direct[DIRECT_PTRS]; will be the pinum
    // probably return 0 if valid, -1 if not?
    FILE *fptr;
    fptr = fopen("image", "r");

    char read_file[64];

    super_t s;

    if (fptr == NULL){
        return -1;
    }
    fgets(read_file, 32, fptr);
    s.inode_bitmap_addr = read_file;


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
        msg_t message;
        super_t s;
        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, (char *)&message, BUFFER_SIZE);

        if (rc < 0){
            return -1;
        }

        switch(message.func){
            case INIT:
                Init(message.hostname, message.port, addr);
                break;
            case LOOKUP:
                Lookup(message.pinum, message.name);
                break;
            case STAT:
                Stat(message.inum, message.m);
                break;
            case WRITE:
                Write(message.inum, message.buffer, message.offset, message.nbytes);
                break;
            case READ:
                Read(message.inum, message.buffer, message.offset, message.nbytes);
                break;
            case CREAT:
                Creat(message.pinum, message.type, message.name);
                break;
            case UNLINK:
                Unlink(message.pinum, message.name);
                break;
            case SHUTDOWN:
                Shutdown();
                break;
        }

        printf("server:: read message");
        if (rc > 0) {
                char reply[BUFFER_SIZE];
                sprintf(reply, "goodbye world");
                rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
            printf("server:: reply\n");
        } 
    }
    return 0; 
}
