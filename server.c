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

int Lookup(int pinum, char *name, super_t *superblock){
    if (pinum < 0 || name == NULL || superblock->num_inodes < pinum){
        return -1;
    }

    // seek to inode bitmap
    super_t s = *superblock;
    int inode_bitmap_addr = s.inode_bitmap_addr; // is it inode_bitmap_addr?

    // check if pinum is allocated
    if (inode_bitmap_addr == 0){
        return -1;
    }

    // seek to inode
    long addr = (long)(fs_img + s.inode_region_addr + (pinum * 4096));
    inode_t inode;
    memcpy(&inode, (void*)addr, sizeof(inode_t));

    // read inode
    if (inode.type != UFS_DIRECTORY){
        return -1;
    }

    // number of directory entries
    int num_dir_ent = (inode.size)/sizeof(dir_ent_t);

    // number of used data blocks
    int num_data_blocks = inode.size/4096;

    // seek to block
    dir_ent_t arr[sizeof(inode.direct)];
    read(fd, &arr, inode.size);

    // iterate through and check 
    for (int i = 0; i < sizeof(arr); i++){ // is it sizeof(arr) and i++??
        if (strcmp(arr[i].name, name) == 0){
            fsync(fd);
            return arr[i].inum;
        }
    }

    fsync(fd);
    return -1;
}
int Stat(int inum, MFS_Stat_t *m){
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
    return 0;
}
int Write(int inum, char *buffer, int offset, int nbytes){
    super_t *superblock; // will be a global variable after next git push

    super_t s = *superblock;

    int i_r_a = s.inode_region_addr;

    long addr = (long)(fs_img + s.inode_region_addr + (inum * 4096));
    inode_t inode;
    memcpy(&inode, (void*)addr, sizeof(inode_t));

    if(inode.type == UFS_DIRECTORY){
        return -1;
    }

    // number of directory entries
    int num_dir_ent = (inode.size)/sizeof(dir_ent_t);

    for (int i = 0; i < num_dir_ent; i++){
        if (1){//inode.direct[i].inum == inum){
            // figure out the actual offset
            // use write() to write buffer of size nbytes to a place in image file
        }
    }

    //inum is an index into the inode //use the func write()

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

    super_t *superblock = (super_t*)fs_img;
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
            case LOOKUP:
                Lookup(message.pinum, message.name, superblock);
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
