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
void *fs_img;
super_t *superblock;

// TODO: fill these up in INIT
typedef struct
{
    int arr[UFS_BLOCK_SIZE];
} inode_bitmap;

typedef struct
{
    int arr[UFS_BLOCK_SIZE];
} data_bitmap;

typedef struct
{
    dir_ent_t entries[128];
} dir_block_t;

unsigned int get_bit(unsigned int *bitmap, int position)
{
    int index = position / 32;
    int offset = 31 - (position % 32);
    return (bitmap[index] >> offset) & 0x1;
}

void set_bit(unsigned int *bitmap, int position)
{
    int index = position / 32;
    int offset = 31 - (position % 32);
    bitmap[index] |= 0x1 << offset;
}

/*
If not allocated, return -1 else return 0
*/
unsigned int checkInodeAllocated(int pinum)
{
    uint bitmapSize = UFS_BLOCK_SIZE / sizeof(unsigned int);
    unsigned int *bits = malloc(sizeof(unsigned int) * bitmapSize); // malloc bitmap :)
    memcpy(bits, fs_img + superblock->inode_bitmap_addr, bitmapSize * sizeof(unsigned int));

    // if not allocated
    if (get_bit(bits, pinum) == 0)
    {
        free(bits);
        return 0;
    }
    free(bits);
    return 1;
}

int Init(char *hostname, int port, struct sockaddr_in addr)
{
    // fill bitmaps
    inode_bitmap i_bitmap;
    //memcpy(&i_bitmap, (void *)superblock->inode_bitmap_addr, sizeof(inode_bitmap));

    data_bitmap d_bitmap;
    //memcpy(&d_bitmap, (void *)superblock->data_bitmap_addr, sizeof(data_bitmap));

    return 0;
}

int Lookup(int pinum, char *name)
{
    if (pinum < 0 || name == NULL || superblock->num_inodes < pinum)
    {
        return -1;
    }
    super_t s = *superblock;
    // check if inode allocated, TODO: ensure bitmap thingy works
    if (checkInodeAllocated(pinum) == 0)
        return -1;

    // seek to inode
    long addr = (long)(fs_img + s.inode_region_addr + (pinum * UFS_BLOCK_SIZE));
    inode_t inode;
    memcpy(&inode, (void *)addr, sizeof(inode_t));

    // read inode
    if (inode.type != MFS_DIRECTORY)
    {
        return -1;
    }

    // number of directory entries? Might have to change
    int num_dir_ent = (inode.size) / sizeof(dir_ent_t);

    // number of used data blocks
    int num_data_blocks = inode.size / UFS_BLOCK_SIZE; // TODO: Fix 500 / 4096 = 0
    if (inode.size % UFS_BLOCK_SIZE != 0)
        num_data_blocks++;

    int x = inode.size; // AMOUNT of BYTES in the file. 4100 bytes 2 data blocks

    dir_ent_t arr[num_dir_ent];

    for (int i = 0; i <= num_data_blocks; i++)
    {
        long curr_data_region = inode.direct[i] * UFS_BLOCK_SIZE;

        dir_block_t db;
        memcpy(&db, (void *)curr_data_region, UFS_BLOCK_SIZE);
        for (size_t i = 0; i < sizeof(db.entries); i++)
        {
            if (strcmp(db.entries[i].name, name) == 0)
            {
                return db.entries[i].inum;
            }
        }
    }
    return -1;
}

int Stat(int inum, MFS_Stat_t *m, s_msg_t server_msg, struct sockaddr_in addr)
{
    // check for invalid inum or invalid mfs_stat
    if (inum < 0 || superblock->num_inodes < inum)
    {
        server_msg.returnCode = -1;
        return -1;
    }

    // check that the inode actually exist looking at the bitmap

    long inode_addr = (long)(fs_img + superblock->inode_region_addr + inum * UFS_BLOCK_SIZE);
    inode_t inode;
    memcpy(&inode, (void *)inode_addr, sizeof(inode_t));
    // Get inode bitmap, does this work?

    uint bitmapSize = UFS_BLOCK_SIZE / sizeof(unsigned int);
    unsigned int *bits = malloc(sizeof(unsigned int) * bitmapSize); // malloc bitmap :)
    memcpy(bits, fs_img + superblock->inode_bitmap_addr, bitmapSize * sizeof(unsigned int));

    // if not allocated
    if (get_bit(bits, inum) == 0)
    {
        server_msg.returnCode = -1;
        return -1;
    }
    free(bits);

    m->size = inode.size;
    m->type = inode.type;
    server_msg.returnCode = 0;
    server_msg.m = m;
    UDP_Write(fd, &addr, (void *)&server_msg, sizeof(server_msg));
    return 0;
}

int Write(int inum, char *buffer, int offset, int nbytes)
{
    long addr = (long)(fs_img + superblock->inode_region_addr + (inum * UFS_BLOCK_SIZE));
    inode_t inode;
    memcpy(&inode, (void *)addr, sizeof(inode_t));
    if (inode.type == MFS_DIRECTORY)
    {
        return -1;
    }
    long curr_data_region = (inode.direct[0] * UFS_BLOCK_SIZE) + offset;
    write(curr_data_region, buffer, nbytes);
    return 0;
}
int Read(int inum, char *buffer, int offset, int nbytes)
{
    return 0;
}
int Creat(int pinum, int type, char *name, s_msg_t server_msg, struct sockaddr_in addr)
{
    // if invalid pnum or invalid/too long of name
    if (pinum < 0 || name == NULL 
    || superblock->num_inodes < pinum || strlen(name) > 28
    || checkInodeAllocated(pinum) == 0)
    {
        return -1;
    }
    // check that the pinode actually exist looking at the bitmap
    long pinode_addr = (long)(fs_img + superblock->inode_region_addr + pinum * 4096);
    inode_t pinode;
    memcpy(&pinode, (void *)pinode_addr, sizeof(inode_t));

    // check if file already exists within parent directory 
    if (Lookup(pinum,name) == 0){
        return 0;
    }
    // file does not exist so create it 
    //pinode.direct
    // find 1st direct ptr that's not empty
    int i = 0;
    for(i = 0; i < 30; i++){
        // if we find unallocated direct pointer allocate it!
        if (pinode.direct[i] == -1){
            long curr_data_region = pinode.direct[i] * UFS_BLOCK_SIZE;
            dir_block_t db;
            memcpy(&db, (void *)curr_data_region, UFS_BLOCK_SIZE);
            for (size_t j = 0; j < sizeof(db.entries); j++){
                if (db.entries[j].inum == -1){ // if we found unallocated inum
                    // try to allocate it and set the name
                    if (pinum + i < superblock->num_inodes){
                        db.entries[j].inum = pinum + i;
                        strcpy(db.entries[j].name, name);
                        // set the type for new inode
                        inode_t inode;
                        inode.type = type;
                        
                        return 0;
                    }
                    return -1;
                }
            }
            return 0;
        }
    }
    // couldn't create file
    if (i == 30){
        return -1;
    }
    return 0;
}
int Unlink(int pinum, char *name)
{
    return 0;
}
int Shutdown()
{
    fsync(fd);
    exit(0);
}

// server code
int main(int argc, char *argv[])
{
    assert(argc == 3);
    int port_num = atoi(argv[1]);
    const char *img_path = argv[2];

    int sd = UDP_Open(port_num);

    if (sd <= 0)
    {
        printf("Failed to bind UDP socket");
        return -1;
    }
    else
    {
        printf("Listening on port %d", port_num);
    }
    assert(sd > -1);
    struct stat file_info;
    int file_fd = open(img_path, O_RDWR | O_APPEND);
    if (fstat(file_fd, &file_info) != 0)
    {
        perror("Fstat failed");
    }

    // TODO: fix parameters
    fs_img = mmap(NULL, sizeof(NULL), MAP_SHARED, PROT_READ | PROT_WRITE, file_fd, 0);

    superblock = (super_t *)fs_img;
    int max_inodes = superblock->inode_bitmap_len * sizeof(unsigned int) * 8;

    unsigned int *inode_bitmap = fs_img + superblock->inode_bitmap_addr * UFS_BLOCK_SIZE;

    inode_t *inodes = fs_img + superblock->inode_region_addr * UFS_BLOCK_SIZE;

    printf("Max inum number is: %i\n", max_inodes);
    printf("Number of inode blocks %i\n", superblock->inode_region_len);
    printf("Number of data blocks: %i\n", superblock->data_region_len);
    printf("Waiting for client messages\n");

    while (1)
    {
        struct sockaddr_in addr;
        msg_t message;
        s_msg_t server_message;
        // super_t s;
        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, (char *)&message, BUFFER_SIZE);

        if (rc < 0)
        {
            return -1;
        }

        switch (message.func)
        {
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
        if (rc > 0)
        {
            char reply[BUFFER_SIZE];
            sprintf(reply, "goodbye world");
            rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
            printf("server:: reply\n");
        }
    }
    return 0;
}
