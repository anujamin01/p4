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

typedef struct {
    dir_ent_t entries[128];
} dir_block_t;

// TODO: fsync() after making some change to the file system
// so probably fsync() at the end of each method?

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

/*
If not allocated, return -1 else return 0
*/
unsigned int checkInodeAllocated(int pinum){
    uint bitmapSize = UFS_BLOCK_SIZE / sizeof(unsigned int);
    unsigned int* bits = malloc(sizeof(unsigned int) * bitmapSize); //malloc bitmap :)
    memcpy(bits, fs_img + superblock->inode_bitmap_addr, bitmapSize * sizeof(unsigned int));

    // if not allocated
    if(get_bit(bits, pinum) == 0){
        free(bits);
        return 0; 
    }
    free(bits);
    return 1;
}

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

    if (pinum < 0 || name == NULL || superblock->num_inodes < pinum){
        return -1;
    }
    super_t s = *superblock;
    //check if inode allocated, TODO: ensure bitmap thingy works
    if(checkInodeAllocated(pinum) == 0) return -1;

    // seek to inode
    long addr = (long)(fs_img + s.inode_region_addr + (pinum * UFS_BLOCK_SIZE));
    inode_t inode;
    memcpy(&inode, (void*)addr, sizeof(inode_t));

    // read inode
    if (inode.type != MFS_DIRECTORY){
        return -1;
    }

    // number of directory entries? Might have to change
    int num_dir_ent = (inode.size)/sizeof(dir_ent_t);

    // number of used data blocks
    int num_data_blocks = inode.size / UFS_BLOCK_SIZE; //TODO: Fix 500 / 4096 = 0 
    if(inode.size % UFS_BLOCK_SIZE != 0) num_data_blocks++;

    int x = inode.size; //AMOUNT of BYTES in the file. 4100 bytes 2 data blocks

    dir_ent_t arr[num_dir_ent];
    
    //4100 - 4096 = 4
    //seek to second data block, reading last 4 bytes
    
    // why can't we just do this only??
    // guessing cause 2 address in direct array could correspond to same thing
    // if so, how to handle that??

    for(int i = 0; i <= num_data_blocks; i++){
        long curr_data_region = inode.direct[i]*UFS_BLOCK_SIZE;

        dir_block_t db;
        memcpy(&db, (void*)curr_data_region, UFS_BLOCK_SIZE);
        for (size_t i = 0; i < sizeof(db.entries); i++)
        {
             if(strcmp(db.entries[i].name, name) == 0){
                return db.entries[i].inum;
             }
        }
    }
    return -1;
}

/*
    int i = 0;
    while(x > 0 && i < num_dir_ent){
        if (inode.direct[i++] == -1){
            continue;
        }
        long curr_data_region = inode.direct[i]*UFS_BLOCK_SIZE;
        //read(curr_data_region, &name, 28);
        dir_ent_t currEnt;
        memcpy(&currEnt, (void*)curr_data_region, 32);
        arr[i] = currEnt;
        i++;
        x-=4096
    }

    for (int j = 0; j < i; j++){
        if (strcmp(arr[j].name,name) == 0){
            return arr[j].inum;
        }
    }
    return -1;
*/

int Stat(int inum, MFS_Stat_t *m, s_msg_t server_msg, struct sockaddr_in addr){ // TODO: include client and server message for parameters
    // check for invalid inum or invalid mfs_stat
    if (inum < 0 || superblock->num_inodes < inum){
        server_msg.returnCode = -1;
        return -1;
    }
    // check that the inode actually exist looking at the bitmap
    
    long inode_addr = (long)(fs_img + superblock->inode_region_addr + inum * UFS_BLOCK_SIZE);
    inode_t inode;
    memcpy(&inode,(void*)inode_addr,sizeof(inode_t));
    //Get inode bitmap, does this work?

    uint bitmapSize = UFS_BLOCK_SIZE / sizeof(unsigned int);
    unsigned int* bits = malloc(sizeof(unsigned int) * bitmapSize); //malloc bitmap :)
    memcpy(bits, fs_img + superblock->inode_bitmap_addr, bitmapSize * sizeof(unsigned int));

    // if not allocated
    if(get_bit(bits, inum) == 0){
        server_msg.returnCode = -1;
        return -1;
    }
    free(bits);
   
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
        server_message.returnCode = -1;        dir_ent_t arr[num_dir_ent];

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
    long addr = (long)(fs_img + superblock->inode_region_addr + (inum * UFS_BLOCK_SIZE));
    inode_t inode;
    memcpy(&inode, (void*)addr, sizeof(inode_t));
    if (inode.type == MFS_DIRECTORY){
        return -1;
    }
    long curr_data_region = superblock->data_region_addr + inode.direct[0];
    return 0;
}
int Read(int inum, char *buffer, int offset, int nbytes){
    return 0;
}
int Creat(int pinum, int type, char *name, s_msg_t server_msg, struct sockaddr_in addr){
    // if invalid pnum or invalid/too long of name
    if (pinum < 0 || name == NULL || superblock->num_inodes < pinum || strlen(name) > BUFFER_SIZE){
        return -1;
    }
    // check that the pinode actually exist looking at the bitmap
    long pinode_addr = (long)(fs_img + superblock->inode_region_addr + pinum * 4096);
    inode_t pinode;
    memcpy(&pinode,(void*)pinode_addr,sizeof(inode_t));
    // pinode has not been allocated DNE
    if (pinode.size == 0){
        //server_msg.returnCode = -1;
        return -1;
    }

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
                Stat(message.inum, message.m, server_message, addr);
                break;
            case WRITE:
                Write(message.inum, message.buffer, message.offset, message.nbytes);
                break;
            case READ:
                Read(message.inum, message.buffer, message.offset, message.nbytes);
                break;
            case CREAT:
                Creat(message.pinum, message.type, message.name, server_message, addr);
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
