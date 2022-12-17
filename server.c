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

inode_t *inodeTable;
unsigned int *inodeBitmap;
unsigned int *dataBitmap;
void * dataRegion;

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

unsigned int get_bit(unsigned int * bitmap, int position)
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

void clear_bit(unsigned int* bitmap, int position){
    int index = position / 32;
    int offset = 31 - (position % 32);
    bitmap[index] &= 0x0 << offset;
}

/*
If not allocated, return -1 else return 0
*/
unsigned int checkInodeAllocated(int pinum)
{
    unsigned int *bits = malloc(UFS_BLOCK_SIZE); // malloc bitmap :)
    memcpy(bits, fs_img + superblock->inode_bitmap_addr * UFS_BLOCK_SIZE, UFS_BLOCK_SIZE);

    // if not allocated
    if (get_bit(bits, pinum) == 0)
    {
        free(bits);
        return 0;
    }
    free(bits);
    return 1;
}

int Init(char *hostname, int port, s_msg_t server_msg, struct sockaddr_in address){
    /*
    long inodeTableAddr = (long)(fs_img + superblock->inode_region_addr * UFS_BLOCK_SIZE);
    //memcpy(inodeTable, inodeTableAddr, );
    
    long inodeBitmapAddr = (long)(fs_img + superblock->inode_bitmap_addr * UFS_BLOCK_SIZE);
    //memcpy(inodeBitmap, inodeBitmapAddr, );
    
    long dataBitmapAddr = (long)(fs_img + superblock->data_bitmap_addr * UFS_BLOCK_SIZE);
    //memcpy(dataBitmap, dataBitmapAddr, );
    */
    return 0;
}

int Lookup(int pinum, char *name, s_msg_t server_msg, struct sockaddr_in address)
{
    printf("IN LOOKUP");
    if (pinum < 0 || name == NULL || superblock->num_inodes < pinum || strlen(name) > 28)
    {
        printf("server pinum\n");
        server_msg.returnCode = -1;
        return -1;
    }
    super_t *s = superblock;
    
    // check if inode allocated, TODO: ensure bitmap thingy works
    if (get_bit(inodeBitmap, pinum) == 0){
        printf("server getbit\n");
        server_msg.returnCode = -1;
        return -1;
    }

    // seek to inode
    inode_t inode = inodeTable[pinum];

    // read inode
    if (inode.type != MFS_DIRECTORY)
    {
        printf("server type\n");
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

    for (int i = 0; i < num_data_blocks; i++)
    {
        long curr_data_region = (long)(fs_img + (inode.direct[i] * UFS_BLOCK_SIZE));

        dir_block_t db;
        memcpy(&db, (void *)curr_data_region, UFS_BLOCK_SIZE);
        for (size_t j = 0; j < num_dir_ent; j++) // num_dir_ent
        {
            if (strcmp(db.entries[j].name, name) == 0)
            {
                printf("SERVER GOOD");
                msync(fs_img, fs_img_size, MS_SYNC);
                server_msg.returnCode = 0;
                server_msg.inode = db.entries[j].inum;
                UDP_Write(sd, &address, (char*)&server_msg, sizeof(server_msg));
                return 0;
            }
        }
    }
    // couldn't find inode
    printf("server end\n");
    msync(fs_img, fs_img_size, MS_SYNC);
    server_msg.inode = -1;
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
    super_t *s = superblock;
    // check if inode allocated, TODO: ensure bitmap thingy works
    if (checkInodeAllocated(pinum) == 0){
        return -1;
    }

    // seek to inode
    inode_t inode = inodeTable[pinum];

    // read inode
    if (inode.type != MFS_DIRECTORY)
    {
        return -1;
    }

    // number of directory entries? Might have to change
    int num_dir_ent = (inode.size) / sizeof(dir_ent_t);

    // number of used data blocks
    int num_data_blocks = inode.size / UFS_BLOCK_SIZE; // TODO: Fix 500 / 4096 = 0
    if (inode.size % UFS_BLOCK_SIZE !=0) {
        num_data_blocks++;
    }

    int x = inode.size; // AMOUNT of BYTES in the file. 4100 bytes = 2 data blocks

    dir_ent_t arr[num_dir_ent];

    for (int i = 0; i < num_data_blocks; i++)
    {
        void *curr_data_region = (void*)(fs_img + (inode.direct[i] * UFS_BLOCK_SIZE));

        dir_block_t db;
        memcpy(&db, curr_data_region, UFS_BLOCK_SIZE);
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
    if (inum < 0 || superblock->num_inodes < inum)
    {
        server_msg.returnCode = -1;
        UDP_Write(file_d, &addr, (void *)&server_msg, sizeof(server_msg));
        return -1;
    }
    inode_t data = inodeTable[inum];
    server_msg.size = data.size; // size of the inode
    server_msg.type = data.type; // type of the inode
    server_msg.returnCode = 0;
    UDP_Write(file_d, &addr, (void *)&server_msg, sizeof(server_msg));
    /*
    // check for invalid inum or invalid mfs_stat


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
    */
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
    printf("Creating %s in %d\n", name, pinum);
    //printf("IN CREAT\n");
    if(superblock->num_inodes < pinum){
        server_msg.returnCode = -1;
        return -1;
    }
    if (get_bit(inodeBitmap,pinum) == 0){
        server_msg.returnCode = -1;
        return -1;
    }
    if (pinum < 0 || name == NULL || strlen(name) > 28)
    {
        server_msg.returnCode = -1;
        return -1;
    }

    super_t s = *superblock;

    // if pinum unallocated 
    // if already created great success!
    if(LookupHelper(pinum, name) != -1){
        server_msg.returnCode = 0;
        return 0;
    }

    if(inodeTable[pinum].type == 1){ //Parent inode is a file 
        return -1;
    }

    // look for spot to put new inode
    int newInode = 0;
    for(int i = 0; i < s.num_inodes; i++){
        int rel = get_bit(inodeBitmap, i);
        if(rel  == 0){ // found an unallocated inode
            newInode = i;
            break;
        }
    }

    // unable to allocate a new inode number
    if(newInode == -1){
        server_msg.returnCode = -1;
        return -1;
    }

    //printf("BEFORE SETBIT\n");
    //get_bit(inodeBitmap, newInode); //mark bit as allocated
    set_bit(inodeBitmap, newInode); //mark bit as allocated
    //printf("AFTER SETBIT\n");

    if(type == 1){ //the new inode is a file 
        inodeTable[newInode].size = 0;
        inodeTable[newInode].type = 1;
        for(int i = 0; i < DIRECT_PTRS; i++){
            inodeTable[newInode].direct[i] = -1;
        } 

        //update parent inode's directory entries
        //pretty sure you dont have to handle the case where a directory block is full.

        inode_t* parent = &inodeTable[pinum];

        // get parent size
        // figure out what the new size will be (parent->size + sizeof(dirent_t))
        // Do we need a new block for the parent? - create one MIGHT BE A TEST CASE
        // Go to parent->direct[whatever] and add a dirent at the end
        // Set parent size to new size

        int entryFound = 0;
        for(int i = 0; i < DIRECT_PTRS; i++){
            if(inodeTable[pinum].direct[i] != -1){
                dir_block_t * parentEntries = (dir_block_t*) (fs_img + inodeTable[pinum].direct[i] * UFS_BLOCK_SIZE);
                for(int j = 0; j < UFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t); j++){
                    if(parentEntries->entries[j].inum == -1){
                        parentEntries->entries[j].inum = newInode;
                        memcpy(parentEntries->entries[j].name, name, strlen(name)); //where we do the strcpy
                        printf("Created entry for %s, %d\n", parentEntries->entries[j].name, newInode);
                        entryFound = 1;
                        inodeTable[pinum].size += 32;
                        break;
                    }
                }
            }
            if(entryFound == 0) break;
        }
    }
    else
    { //the new inode is a directory
        inodeTable[newInode].size = 2 * sizeof(dir_ent_t);
        inodeTable[newInode].type = 0;

        // find unallocated datablock via bitmap 
        int n = -1;

        // find first unallocated block and mark it to be unallocated
        for(n= 0; n < s.num_data; n++){
            if(get_bit(dataBitmap, n) == 0){ 
                set_bit(dataBitmap, n);
                break;
            }
        }
        if (n == -1){
            server_msg.returnCode = -1;
            return -1;
        }

        //pointer to the newly allocated data block
        dir_block_t * dir;
        void * dir_addr = (void *)(fs_img + s.data_region_addr * UFS_BLOCK_SIZE + n * UFS_BLOCK_SIZE);
        dir = (dir_block_t*) dir_addr;
        //printf("%p\n", &dir);
        // set name and inode numbers 
        dir->entries[0].inum = newInode;
        strcpy(dir->entries[0].name, ".");
        dir->entries[1].inum = pinum;
        strcpy(dir->entries[1].name, "..");
    
        inodeTable[newInode].direct[0] = n;

        for(int i = 2; i < UFS_BLOCK_SIZE / 32; i++){
            dir->entries[i].inum = -1;
        }
        //memcpy(dir_addr, &dir, sizeof(dir_block_t));
        for(int i = 1; i < DIRECT_PTRS; i++){
            inodeTable[newInode].direct[i] = -1;
        }
    }

    msync(fs_img, fs_img_size, MS_SYNC);
    server_msg.returnCode = 0;
    UDP_Write(sd, &addr, (char*)&server_msg, sizeof(server_msg));
    return 0;
}

int Unlink(int pinum, char *name, s_msg_t server_msg)
{
    /*
    TODO:
    removes the file or directory name from the directory specified by pinum. 0 on success, -1 on failure.
    Failure modes: pinum does not exist, directory is NOT empty. Note that the name not existing is NOT 
    a failure by our definition (think about why this might be).
    */
    int childInum;

    // invalid name or inode
    if (pinum < 0 || pinum > superblock->num_inodes || name == NULL || strlen(name) > 28){
        server_msg.returnCode = -1;
        return -1;
    }
    if((childInum = LookupHelper(pinum, name)) == -1){ 
        server_msg.returnCode = -1;
        return -1;
    }

    inode_t child = inodeTable[childInum];
    
    if(child.type == 1){ // child is a file
        for(int i = 0; i < DIRECT_PTRS; i++){
            if(child.direct[i] != -1){ //setting direct poitners to zero.
                clear_bit(dataBitmap, child.direct[i]);
            }
        }
    }
    else{ //it is a directory
        if(child.size > 2 * sizeof(MFS_DirEnt_t)){ //we have more directory entries than "." and ".."
            return -1;
        }
        for(int i = 0; i < DIRECT_PTRS; i++){ //clear child datablocks
            clear_bit(dataBitmap, child.direct[i]);
        }
    }
    int found = -1;

    //Remove child directory entry from parent 
    for(int i = 0; i < DIRECT_PTRS; i++){
        if(inodeTable[pinum].direct[i] != -1){
            dir_block_t* parentDirEntries = (dir_block_t*)(fs_img + superblock->data_region_addr * UFS_BLOCK_SIZE + inodeTable[pinum].direct[i] * UFS_BLOCK_SIZE); 

            for(int j = 0; j < 128; j++){
                if(strcmp(parentDirEntries->entries[j].name, name) == 0){
                    parentDirEntries->entries[j].inum = -1;
                    found = 1;
                    break;
                }
            }
        }
        if(found == 1){
            inodeTable[pinum].size -= 32;
            break;
        }
    }
    clear_bit(inodeBitmap, childInum); 

    msync(fs_img, fs_img_size, MS_SYNC); //sync memory mapped file back to disk.
    return 0;
}

int Shutdown(s_msg_t server_msg, struct sockaddr_in addr)
{
    server_msg.returnCode = 0;
    UDP_Write(sd, &addr, (void *)&server_msg, sizeof(server_msg));
    fsync(file_d);
    close(file_d);
    UDP_Close(sd);
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
    fs_img = mmap(NULL, file_info.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_d, 0); // was another_file_fd;
    if (fs_img == MAP_FAILED)
    {
        perror("mmap failed");
        exit(1);
    }
    superblock = (super_t *)fs_img;

    inodeTable = (inode_t*)(fs_img + superblock->inode_region_addr * UFS_BLOCK_SIZE);
    inodeBitmap = (unsigned int *)(fs_img + superblock->inode_bitmap_addr * UFS_BLOCK_SIZE);
    dataBitmap = (unsigned int *)(fs_img  + superblock->data_bitmap_addr * UFS_BLOCK_SIZE);
    dataRegion = (fs_img + superblock->data_region_addr * UFS_BLOCK_SIZE);
    //printf("IN SEVER1\n");
    while (1)
    {
        struct sockaddr_in addr;
        msg_t message;
        s_msg_t server_message;

        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, (char *)&message, sizeof(msg_t));
        printf("IN SERVER\n");
        if (rc < 0)
        {
            printf("mission failed\n");
            return -1;
        }
        /*
        printf("Reciving variables in server...\n");
        printf("pinum %d\n",message.pinum);
        printf("pinum %d\n",message.type);
        printf("pinum %s\n",message.name);
        printf("IN SEVER2\n");
        */
        switch (message.func)
        {
        case INIT:
            //Init(message.hostname, message.port, server_message, addr);
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
            //printf("CALLING CREAT\n");
            Creat(message.pinum, message.type, message.name, server_message, addr);
            break;
        case UNLINK:
            Unlink(message.pinum, message.name, server_message);
            break;
        case SHUTDOWN:
            Shutdown(server_message, addr);
            break;
        }

        /*
        printf("server:: read message");
        if (rc > 0)
        {
            rc = UDP_Write(sd, &addr, (char*)&server_message, sizeof(s_msg_t));
        }
        */
    }
    return 0;
}
