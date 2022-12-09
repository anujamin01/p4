#include "udp.h"
#include "ufs.h"
#include "mfs.h"
#include "msg.h"
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#define BUFFER_SIZE (1000)

int fd = -1;
void * fs_img;
super_t *superblock;
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

    //FILE *fptr;
    //fptr = fopen("image", "r");

    //char read_file[64];

    //super_t s;
    /*
    if (fptr == NULL){; (in blocks)
    }
    fgets(read_file, 32, fptr);
    //s.inode_bitmap_addr = atoi(read_file);
    */

    if (pinum < 0 || name == NULL || superblock->num_inodes < pinum){
        return -1;
    }
    super_t s = *superblock;
    int inode_region_addr = s.inode_region_addr;
    return 0;
}
int Stat(int inum, MFS_Stat_t *m, s_msg_t server_msg, struct sockaddr_in addr){ // TODO: include client and server message for parameters
    // check for invalid inum or invalid mfs_stat
    if (inum < 0 || superblock->num_inodes < inum){
        server_msg.returnCode = -1;
        return -1;
    }
    // check that the inode actually exist looking at the bitmap
    long inode_addr = (long)(fs_img + superblock->inode_region_addr + inum * 4096);
    inode_t inode;
    memcpy(&inode,(void*)inode_addr,sizeof(inode_t));
    // inode has not been allocated DNE
    if (inode.size == 0){
        server_msg.returnCode = -1;
        return -1;
    }
    m->size = inode.size;
    m->type = inode.type;
    server_msg.returnCode = 0;
    server_msg.m = m;
    UDP_Write(fd, &addr, (void*)&server_msg, sizeof(server_msg)); 
    return 0;
    // need to grab the data 
    /*
    int inum = message.inum; // message.stat.inum;
    if (message.inum >= max_inodes){
        printf("Got invalid inum\n");
        server_message.returnCode = -1;
    }
    else{
        // check that the inode actually exist looking at the bitmap
        server_message.type = inodes[inum].type;
        server_message.returnCode = 0;
    }
    UDP_Write(fd, &addr, (void*)&server_message, sizeof(server_message));
    */
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

unsigned int get_bit(unsigned int *bitmap, int position) {
   int index = position / 32;
   int offset = 31 - (position % 32);
   return (bitmap[index] >> offset) & 0x1;
}

void set_bit(unsigned int *bitmap, int position) {
   int index = position / 32;
   int offset = 31 - (position % 32);
   bitmap[index] |=  0x1 << offset;
}

// server code
int main(int argc, char *argv[]) {
    assert(argc == 3);
    int port_num = atoi(argv[1]);
    const char* img_path = argv[2];

    int sd = UDP_Open(port_num);

    if (sd <= 0){
        printf("Failed to bind UDP socket");
        return -1;
    } else{
        printf("Listening on port %d", port_num);
    }
    assert(sd > -1);
    struct stat file_info;
    int file_fd = open(img_path, O_RDWR | O_APPEND);
    if (fstat(file_fd, &file_info) != 0){
        perror("Fstat failed");
    }

    // TODO: fix parameters
    fs_img = mmap(NULL, sizeof(NULL), MAP_SHARED, PROT_READ | PROT_WRITE, file_fd, 0);

    superblock = (super_t*)fs_img;
    int max_inodes = superblock->inode_bitmap_len * sizeof(unsigned int) * 8;

    unsigned int *inode_bitmap = fs_img + superblock->inode_bitmap_addr * UFS_BLOCK_SIZE;

    inode_t *inodes = fs_img + superblock->inode_region_addr * UFS_BLOCK_SIZE;
    
    printf("Max inum number is: %i\n", max_inodes);
    printf("Number of inode blocks %i\n",superblock->inode_region_len);
    printf("Number of data blocks: %i\n", superblock->data_region_len);
    printf("Waiting for client messages\n");

    while (1) {
        struct sockaddr_in addr;
        msg_t message;
        s_msg_t server_message;
        //super_t s;
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
                Stat(message.inum, message.m,server_message,addr);
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
