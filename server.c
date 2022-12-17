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
    return 0;
}

int Lookup(int pinum, char *name, s_msg_t server_msg, struct sockaddr_in address)
{
    if (pinum < 0 || name == NULL || superblock->num_inodes < pinum || strlen(name) > 28)
    {
        server_msg.returnCode = -1;
        return -1;
    }
    super_t *s = superblock;
    
    // check if inode allocated, TODO: ensure bitmap thingy works
    if (get_bit(inodeBitmap, pinum) == 0){
        server_msg.returnCode = -1;
        return -1;
    }

    // seek to inode
    inode_t inode = inodeTable[pinum];

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

    for (int i = 0; i < num_data_blocks; i++)
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
                return 0;
            }
        }
    }
    // couldn't find inode
    msync(fs_img, fs_img_size, MS_SYNC);
    server_msg.returnCode = -1;
    return -1;
}

int Stat(int inum, s_msg_t server_msg, struct sockaddr_in addr)
{
    if(get_bit(inodeBitmap, inum)==0){
        server_msg.returnCode = -1;
        return -1;
    }
    inode_t *curr_inode = inodeTable + inum;
    server_msg.size = curr_inode->size;
    server_msg.type = curr_inode->type;
    server_msg.returnCode = 0;
    return 0;
}

int Write(int inum, char *buffer, int offset, int nbytes, s_msg_t server_msg, struct sockaddr_in address)
{
    if(inum < 1 || (offset/UFS_BLOCK_SIZE)>=DIRECT_PTRS || get_bit(inodeBitmap, inum) == 0 || (offset+nbytes)>UFS_BLOCK_SIZE*DIRECT_PTRS){
        server_msg.returnCode = -1;
        return -1;
    }
    inode_t *curr_inode = inodeTable + inum;
    if(curr_inode->type != MFS_REGULAR_FILE){ // only directory allowed
        server_msg.returnCode = -1;
        return -1;
    }
    int nextBlock;
    int write_rc;
    if(curr_inode->size == 0){ // no block
        int unallocBit = -1;
        for (int i = 0; i < 1024; i++) { // find unallocated bit
            if (get_bit(dataBitmap, i) == 0) {
                unallocBit = i; 
            }
        }
        if(unallocBit == -1){
            server_msg.returnCode = -1;
            return -1;
        }
        set_bit(dataBitmap, unallocBit);
        curr_inode->direct[0] = superblock->data_region_addr + unallocBit;
        nextBlock = curr_inode->direct[0]*UFS_BLOCK_SIZE;

    }


    ////////
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
    if(superblock->num_inodes < pinum){
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
    if(Lookup(pinum, name, server_msg, addr) != -1){
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

    set_bit(inodeBitmap, newInode); //mark bit as allocated

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
        // Do we need a new block for the parent? - create one
        // Go to parent->direct[whatever] and add a dirent at the end
        // Set parent size to new size

        int entryFound = 0;
        for(int i = 0; i < DIRECT_PTRS; i++){
            if(inodeTable[pinum].direct[i] != -1){
                dir_block_t * parentEntries = (dir_block_t*) fs_img + inodeTable[pinum].direct[i] * UFS_BLOCK_SIZE;
                for(int j = 0; j < UFS_BLOCK_SIZE / sizeof(MFS_DirEnt_t); j++){
                    if(parentEntries->entries[j].inum == -1){
                        parentEntries->entries[j].inum = newInode;
                        memcpy(&(parentEntries->entries[j].name), &name, 28);
                        entryFound = 1;
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

     // get data bitmap
        long bitmapAddr = (long)(fs_img + s.data_bitmap_addr * UFS_BLOCK_SIZE);
        // find first unallocated block and mark it to be unallocated
        for(n= 0; n < s.num_data; n++){
            if(get_bit((unsigned int*)bitmapAddr, n) == 0){ 
                set_bit((unsigned int*)(fs_img + s.data_bitmap_addr * UFS_BLOCK_SIZE), n);
                break;
            }
        }
        if (n == -1){
            server_msg.returnCode = -1;
            return -1;
        }

        //pointer to the newly allocated data block
        dir_block_t dir;
        void * dir_addr = (void *)(fs_img + s.data_region_addr * UFS_BLOCK_SIZE + n * UFS_BLOCK_SIZE);
        memcpy(&dir, dir_addr, sizeof(dir_block_t));
        // set name and inode numbers 
        dir.entries[0].inum = newInode;
        strcpy(dir.entries[0].name, ".");
        dir.entries[1].inum = pinum;
        strcpy(dir.entries[1].name, "..");

        inodeTable[newInode].direct[0] = n;

        for(int i = 2; i < UFS_BLOCK_SIZE / 32; i++){
            dir.entries[i].inum = -1;
        }
        memcpy(dir_addr, &dir, sizeof(dir_block_t));
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
    while (1)
    {
        struct sockaddr_in addr;
        msg_t message;
        s_msg_t server_message;

        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, (char *)&message, sizeof(msg_t));
        if (rc < 0)
        {
            return -1;
        }
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
            Write(message.inum, message.buffer, message.offset, message.nbytes, server_message, addr);
            break;
        case READ:
            Read(message.inum, message.buffer, message.offset, message.nbytes);
            break;
        case CREAT:
            Creat(message.pinum, message.type, message.name, server_message, addr);
            break;
        case UNLINK:
            Unlink(message.pinum, message.name, server_message);
            break;
        case SHUTDOWN:
            Shutdown(server_message, addr);
            break;
        }
    }
    return 0;
}
