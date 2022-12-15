#include "udp.h"
#include "ufs.h"
#include "mfs.h"
#include "msg.h"
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#define BUFFER_SIZE (1000)

int file_d = -1;
void *fs_img;
super_t *superblock;
int sd;
int fs_img_size;
inode_t inodes;

// TODO: use fsync() in some functions
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
    unsigned int *bits = malloc(UFS_BLOCK_SIZE); // malloc bitmap :)
    memcpy(bits, fs_img + superblock->inode_bitmap_addr, UFS_BLOCK_SIZE);

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

    fsync(file_d);
    return 0;
}

int Lookup(int pinum, char *name, s_msg_t server_msg, struct sockaddr_in address)
{
    if (pinum < 0 || name == NULL || superblock->num_inodes < pinum || strlen(name) > 28)
    {
        server_msg.returnCode = -1;
        return -1;
    }
    super_t s = *superblock;
    // check if inode allocated, TODO: ensure bitmap thingy works
    if (checkInodeAllocated(pinum) == 0){
        server_msg.returnCode = -1;
        return -1;
    }

    // seek to inode
    long addr = (long)(fs_img + s.inode_region_addr*UFS_BLOCK_SIZE + (pinum * sizeof(inode_t)));
    inode_t inode;
    memcpy(&inode, (void *)addr, sizeof(inode_t));

    // read inode
    if (inode.type != MFS_DIRECTORY)
    {
        server_msg.returnCode = -1;
        return -1;
    }

    // number of directory entries? Might have to change
    int num_dir_ent = (inode.size) / sizeof(dir_ent_t);

    // number of used data blocks
    int num_data_blocks = inode.size / UFS_BLOCK_SIZE; // TODO: Fix 500 / 4096 = 0
    if (inode.size % UFS_BLOCK_SIZE != 0) // fixed here
        num_data_blocks++;

    int x = inode.size; // AMOUNT of BYTES in the file. 4100 bytes = 2 data blocks

    dir_ent_t arr[num_dir_ent];

    for (int i = 0; i <= num_data_blocks; i++)
    {
        long curr_data_region = (long)(fs_img + (inode.direct[i] * UFS_BLOCK_SIZE));

        dir_block_t db;
        memcpy(&db, (void *)curr_data_region, UFS_BLOCK_SIZE);
        for (size_t j = 0; j < num_dir_ent; j++) // num_dir_ent
        {
            if (strcmp(db.entries[j].name, name) == 0)
            {
                msync(fs_img, fs_img_size, MS_SYNC);
                server_msg.returnCode = 0;
                server_msg.inode = db.entries[j].inum;
                UDP_Write(sd, &address, (char*)&server_msg, sizeof(server_msg));
                return 0;
            }
        }
    }
    msync(fs_img, fs_img_size, MS_SYNC);
    server_msg.returnCode = -1;
    UDP_Write(sd, &address, (char*)&server_msg, sizeof(server_msg));
    return -1;
}

int LookupHelper(int pinum, char *name)
{
    if (pinum < 0 || name == NULL || superblock->num_inodes < pinum || strlen(name) > 28)
    {
        return -1;
    }
    super_t s = *superblock;
    // check if inode allocated, TODO: ensure bitmap thingy works
    if (checkInodeAllocated(pinum) == 0){
        return -1;
    }

    // seek to inode
    long addr = (long)(fs_img + s.inode_region_addr*UFS_BLOCK_SIZE + (pinum * sizeof(inode_t)));
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
    if (inode.size % UFS_BLOCK_SIZE != 0) // fixed here
        num_data_blocks++;

    int x = inode.size; // AMOUNT of BYTES in the file. 4100 bytes = 2 data blocks

    dir_ent_t arr[num_dir_ent];

    for (int i = 0; i <= num_data_blocks; i++)
    {
        long curr_data_region = (long)(fs_img + (inode.direct[i] * UFS_BLOCK_SIZE));

        dir_block_t db;
        memcpy(&db, (void *)curr_data_region, UFS_BLOCK_SIZE);
        for (size_t j = 0; j < num_dir_ent; j++) // num_dir_ent
        {
            if (strcmp(db.entries[j].name, name) == 0)
            {
                return 0;
            }
        }
    }
    return -1;
}

int Stat(int inum, s_msg_t server_msg, struct sockaddr_in addr)
{
    // check for invalid inum or invalid mfs_stat
    if (inum < 0 || superblock->num_inodes < inum)
    {
        server_msg.returnCode = -1;
        return -1;
    }

    // check that the inode actually exist looking at the bitmap
    // use checkInodeAllocated()

    long inode_addr = (long)(fs_img + superblock->inode_region_addr + inum * UFS_BLOCK_SIZE);
    inode_t inode;
    memcpy(&inode, (void *)inode_addr, sizeof(inode_t));
    // Get inode bitmap, does this work?

    unsigned int bitmapSize = UFS_BLOCK_SIZE / sizeof(unsigned int);
    unsigned int *bits = malloc(sizeof(unsigned int) * bitmapSize); // malloc bitmap :)
    memcpy(bits, fs_img + superblock->inode_bitmap_addr, bitmapSize * sizeof(unsigned int));

    // if not allocated
    if (get_bit(bits, inum) == 0)
    {
        server_msg.returnCode = -1;
        return -1;
    }
    free(bits);
    
    server_msg.returnCode = 0;
    server_msg.m.size = inode.size;
    server_msg.m.type = inode.type;
    UDP_Write(file_d, &addr, (void *)&server_msg, sizeof(server_msg));
    
    fsync(file_d);
    return 0;
}

int Write(int inum, char *buffer, int offset, int nbytes)
{
    if (inum < 0 || superblock->num_inodes < inum || nbytes < 0 || offset < 0)
    {
        return -1;
    }
    long addr = (long)(fs_img + superblock->inode_region_addr + (inum * UFS_BLOCK_SIZE));
    inode_t inode;
    memcpy(&inode, (void *)addr, sizeof(inode_t));
    if (inode.type == MFS_DIRECTORY)
    {
        return -1;
    }

    // TODO:
    // the index in direct is: offset / 4096
    // first block: n = (offset % 4096 + nbytes) - 4096
    // next blocks: nbytes -  n
    // what if we need to write to next block?
    // what if the next block is not allocated?

    int offset_index = offset / UFS_BLOCK_SIZE;

    // start of data region where we need to write
    int start_data_region = inode.direct[offset_index];

    // how much to write in the next block
    long next_block_amount = (long)((offset % UFS_BLOCK_SIZE) + nbytes) - UFS_BLOCK_SIZE;

    // how much to write in the first block
    long first_block_amount = nbytes - next_block_amount;

    // write to first block
    write(start_data_region, buffer, first_block_amount);

    while(next_block_amount > 0){
        if (inode.direct[offset_index + 1] == -1){
            // allocate new block
            // inode.direct[offset_index + 1] = allocateBlock();
            inode.direct[offset_index + 1] = superblock->data_region_addr + (offset_index + 1);
        } else{
            // write to next block
            write(inode.direct[offset_index + 1], buffer, next_block_amount);
        }
        next_block_amount -= UFS_BLOCK_SIZE;
    }

    fsync(file_d);
    return 0;
}

int Read(int inum, char *buffer, int offset, int nbytes)
{
    long addr = (long)(fs_img + superblock->inode_region_addr + (inum * UFS_BLOCK_SIZE));
    inode_t inode;
    memcpy(&inode, (void *)addr, sizeof(inode_t));
    if (inode.type == MFS_DIRECTORY)
    {
        return -1;
    }
    long curr_data_region = (inode.direct[0] * UFS_BLOCK_SIZE) + offset;
    read(curr_data_region, buffer, nbytes);

    fsync(file_d);
    return 0;
}


int Creat(int pinum, int type, char *name, s_msg_t server_msg, struct sockaddr_in addr)
{
    // if invalid pnum or invalid/too long of name
    if (pinum < 0 || name == NULL || superblock->num_inodes < pinum || strlen(name) > BUFFER_SIZE)
    {
        server_msg.returnCode = -1;
        return -1;
    }

    super_t s = *superblock;

    // if pinum unallocated 
    if(checkInodeAllocated(pinum) == 0){
        server_msg.returnCode = -1;
        return -1;
    }

    // if already created great success!
    if(LookupHelper(pinum, name) != -1){
        server_msg.returnCode = 0;
        return 0;
    }

    // get address of start of pinum
    long address = (long)(fs_img + s.inode_region_addr * UFS_BLOCK_SIZE + (pinum * sizeof(inode_t)));
    inode_t pinode;
    memcpy(&pinode, (void*)address, sizeof(inode_t));

    // get inode bitmap
    unsigned int bits[UFS_BLOCK_SIZE / sizeof(unsigned int)];
    memcpy(bits, fs_img + s.inode_bitmap_addr * UFS_BLOCK_SIZE, UFS_BLOCK_SIZE);
    
    int inode_num = -1;
    for(int i = 0; i < s.num_inodes; i++){
        if(get_bit((unsigned int *)bits, i) == 0){
            inode_num = i;
            set_bit((unsigned int*) bits, inode_num);
            break;
        }
    }

    // unable to allocate a new inode number
    if(inode_num == -1){
        server_msg.returnCode = -1;
        return -1;
    }

    //Write out the set inode bitmap to disk
    pwrite(file_d, &bits, UFS_BLOCK_SIZE, UFS_BLOCK_SIZE * s.inode_bitmap_addr);

    // STARTING NEW ONE...
    int spaceFound = 0;
    dir_block_t *dir_entries;
    int alreadyExists = 0;

    // Writing to parent inode's directory entries, if space already exists
    for (int j = 0; j < 30; j++)
    {
        if (pinode.direct[j] != -1)
        {
            long curr_data_region = (long)(fs_img + (pinode.direct[j] * UFS_BLOCK_SIZE));

            dir_entries = (dir_block_t *) (fs_img + (pinode.direct[j] * 4096));

            for (int k = 0; k < 128; k++)
            {
                if(dir_entries->entries[k].inum != -1){
                    if(strcmp(dir_entries->entries->name, name) == 0)
                    {
                        server_msg.returnCode = 0;
                        alreadyExists = 1;
                        return 0;
                    }
                }
                else
                {
                    dir_entries->entries[k].inum = inode_num;
                    strcpy(dir_entries->entries[k].name,name);
                    spaceFound = 1;
                    break;
                }
            }   
        }
    }

    // create inode and fill with relevant info
    inode_t new_inode_t;
    new_inode_t.type = type;

     // get data bitmap
    unsigned int data_bits[UFS_BLOCK_SIZE / sizeof(unsigned int)];
    memcpy(data_bits, fs_img + s.data_bitmap_addr * UFS_BLOCK_SIZE, UFS_BLOCK_SIZE);
    /*
    First I loop through and check if there are any datablocks that are not -1 (meaning they are allocated)

    */

   // why do we have first loop;
    for (int l = 0; l < 30; l++){ // DIRECT_PTRS probably 30
        if (pinode.direct[l] == -1){
            int data_num = -1;
            
            int n = 0;
            // find first unallocated data
            for(n = 0; n < s.num_data; n++)
            {
                if(get_bit((unsigned int *)data_bits, n) == 0){//!Allocated
                    // I think this sets the bit in data bitmap to 1
                    set_bit((unsigned int*)(fs_img + s.data_bitmap_addr*4096), n);
                    break;
                }
            }
        }
    }

    inode_t* newInode = fs_img + inode_num * UFS_BLOCK_SIZE + s.inode_region_addr * UFS_BLOCK_SIZE;
    newInode->type = type;
    dir_block_t *new_dir_entries;
    if(type == MFS_DIRECTORY)
    {
        newInode->size = 2*sizeof(dir_ent_t);
        
        // TODO: fix this
        //Find unallocated datablock via bitmap
        //set a bit in the bitmap
        //need to find a data block
        int n = 0;
        for(n = 0; n < s.num_data; n++){
            if(get_bit((unsigned int *)data_bits, n) == 0)
            { //!Allocated
                // I think this sets the bit in data bitmap to 1
                set_bit((unsigned int*)(fs_img + s.data_bitmap_addr*4096), n);
            }
        }

        new_dir_entries = (dir_block_t*)(fs_img + (s.data_region_addr*4096) + (n * UFS_BLOCK_SIZE));

        new_dir_entries->entries[0].inum = inode_num;
        strcpy(new_dir_entries->entries[0].name, ".");

        new_dir_entries->entries[1].inum = pinum;
        strcpy(new_dir_entries->entries[1].name, "..");

        for (int i = 2; i < 128; i++)
	    new_dir_entries->entries[i].inum = -1;
    }
    else
    {
        newInode->size = 0;

        // ??
        for (int i = 0; i < 128; i++)
	    new_dir_entries->entries[i].inum = -1;
    }

    msync(fs_img, fs_img_size, MS_SYNC);
    server_msg.returnCode = 0;
    UDP_Write(sd, &addr, (char*)&server_msg, sizeof(server_msg));
    return 0;
}

int Unlink(int pinum, char *name)
{
    /*
    TODO:
    removes the file or directory name from the directory specified by pinum. 0 on success, -1 on failure.
    Failure modes: pinum does not exist, directory is NOT empty. Note that the name not existing is NOT 
    a failure by our definition (think about why this might be).
    */

    // invalid name or inode
    if (pinum < 0 || pinum > superblock->num_inodes || name == NULL || strlen(name) > 28){
        return -1;
    }



    fsync(file_d);
    return 0;
}

int Shutdown(s_msg_t server_msg, struct sockaddr_in addr)
{
    server_msg.returnCode = 0;
    UDP_Write(sd, &addr, (void *)&server_msg, sizeof(server_msg));
    fsync(file_d);
    close(file_d);
    exit(0);
}

void intHandler(int dummy) {
    UDP_Close(sd);
    exit(130);
}

// server code
int main(int argc, char *argv[])
{
    signal(SIGINT, intHandler);
    assert(argc == 3);
    int port_num = atoi(argv[1]);
    const char *img_path = argv[2];

    //port_num = 11011;
    sd = UDP_Open(port_num);

    if (sd <= 0)
    {
        return -1;
    }
    assert(sd > -1);
    struct stat file_info;
    int another_file_fd = open(img_path, O_RDWR | O_APPEND);
    file_d = another_file_fd;
    if (fstat(file_d, &file_info) != 0) // was another_file_fd
    {
        perror("Fstat failed");
    }

    // TODO: fix parameters
    struct stat st;
    fs_img = mmap(NULL, sizeof(NULL), MAP_SHARED, PROT_READ | PROT_WRITE, file_d, 0); // was another_file_fd;
    fs_img_size = stat(fs_img,&st);
    superblock = (super_t *)fs_img;
    int max_inodes = superblock->inode_bitmap_len * sizeof(unsigned int) * 8;

    unsigned int *inode_bitmap = fs_img + superblock->inode_bitmap_addr * UFS_BLOCK_SIZE;

    //inodes = fs_img + superblock->inode_region_addr * UFS_BLOCK_SIZE;
    /*
    printf("Max inum number is: %i\n", max_inodes); 
    printf("Number of inode blocks %i\n", superblock->inode_region_len);
    printf("Number of data blocks: %i\n", superblock->data_region_len);
    printf("Waiting for client messages\n");
    */

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
            //Init(message.hostname, message.port, addr);
            break;
        case LOOKUP:
            Lookup(message.pinum, message.name, server_message, addr);
            break;
        case STAT:
            Stat(message.inum, server_message, addr);
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
            Shutdown(server_message, addr);
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