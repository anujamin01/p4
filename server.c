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
struct sockaddr_in addr;
void *fs_img;
super_t *superblock;
int sd;
int fs_img_size;
struct stat file_info;
int dot_edge_case;

inode_t *inodeTable;
unsigned int *inodeBitmap;
unsigned int *dataBitmap;
void * dataRegion;
dir_ent_t *rootDir;
inode_t *rootInode;

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

int Init(char *hostname, int port, msg_t *server_msg){
    return 0;
}

int Lookup(int pinum, char *name, msg_t *server_msg)
{

    // invalid name or invalid pinum
    if (pinum < 0 || name == NULL || superblock->num_inodes < pinum || strlen(name) > 28 
        || get_bit(inodeBitmap, pinum) == 0)
    {
        return -1;
    }
 
     //get inode of parent   
    inode_t *pinode = inodeTable + pinum; 
    // cannot lookup inside file
    if (pinode->type == 1) {
        return -1; 
    }  

    // iterate through direct block
    dir_ent_t *currBlock = malloc(UFS_BLOCK_SIZE);
    int i = 0;

    while(i < 30){
        // examine allocated region
        if(pinode->direct[i] != -1){
            long addr = pinode->direct[i] * UFS_BLOCK_SIZE;

            if(addr > 0){
                // address a datablock
                int testSeek = lseek(file_d, addr, SEEK_SET);
                if(testSeek < 0){
                    return -1;
                }

                // read the datablock
                int testRead = read(file_d, currBlock, UFS_BLOCK_SIZE);
                if (testRead < 0){
                    return -1;
                }
                int j = 0;
                printf("name: %s\n", name);
                if(pinum != 0){
                    if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0){
                        printf("IN STRCMP IF\n");
                        j = 2;
                    }
                }
                printf("currBlock[0].name, currBlock[0].inum: %s, %d...\n", currBlock[0].name, currBlock[0].inum);
                printf("currBlock[1].name, currBlock[1].inum: %s, %d...\n", currBlock[1].name, currBlock[1].inum);
                printf("currBlock[1].name, currBlock[1].inum: %s, %d...\n", currBlock[2].name, currBlock[2].inum);
                // for every directory in datablock
                while(j < 128){
                    dir_ent_t currEntry = currBlock[j];

                    // check allocated inum
                    if (currEntry.inum != -1){
                        if (strcmp(currEntry.name,name) == 0){
                            printf("IN LOOKUP\n");
                            return currEntry.inum;
                        }
                    }
                    j++;
                }
            }
        }
        i++;
    }
    free(currBlock);
    return -1; // not found
}

int Stat(int inum, msg_t *server_msg)
{
    if(get_bit(inodeBitmap, inum)==0){
        server_msg->returnCode = -1;
        return -1;
    }
    inode_t *curr_inode = inodeTable + inum;
    server_msg->size = curr_inode->size;
    server_msg->type = curr_inode->type;
    server_msg->returnCode = 0;
    return 0;
}

int Write(int inum, char *buffer, int offset, int nbytes, msg_t *server_msg)
{
    if(inum < 1 || (offset/UFS_BLOCK_SIZE)>=DIRECT_PTRS || get_bit(inodeBitmap, inum) == 0 || (offset+nbytes)>UFS_BLOCK_SIZE*DIRECT_PTRS){
        return -1;
    }
    inode_t *curr_inode = inodeTable + inum;
    if(curr_inode->type != MFS_REGULAR_FILE){ // only directory allowed
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
            return -1;
        }
        set_bit(dataBitmap, unallocBit);
        curr_inode->direct[0] = superblock->data_region_addr + unallocBit;
        nextBlock = curr_inode->direct[0]*UFS_BLOCK_SIZE;
        write_rc = pwrite(file_d, buffer, nbytes, nextBlock);
    }
    else{ // at least one block
        int dir_index = offset/UFS_BLOCK_SIZE;
        if(dir_index>=DIRECT_PTRS)
        {
            return -1;
        }
        if(curr_inode->direct[dir_index] == -1)
        {
            int unallocBit = -1;
            for (int i = 0; i < 1024; i++) { // find unallocated bit
                if (get_bit(dataBitmap, i) == 0) {
                    unallocBit = i; 
                }
            }
            if(unallocBit == -1)
            {
                return -1;
            }
            set_bit(dataBitmap, unallocBit); //curr_inode->direct[dir_index]);
            curr_inode->direct[dir_index] = superblock->data_region_addr + unallocBit;
        }

        nextBlock = curr_inode->direct[dir_index] * UFS_BLOCK_SIZE + (offset%UFS_BLOCK_SIZE);

        write_rc = pwrite(file_d, buffer, nbytes, nextBlock);
    }
    if(write_rc == -1 || write_rc != nbytes){
        return -1;
    }

    curr_inode->size = offset + nbytes;
    
    int rc = msync(fs_img, (int)file_info.st_size, MS_SYNC);
    assert(rc > -1);
    rc = fsync(file_d);
    assert(rc>-1);
    return 0;
}

int Read(int inum, char *buffer, int offset, int nbytes, msg_t *server_message)
{
    server_message->returnCode = 0;
    if (inum >= 0){
        if(get_bit(inodeBitmap, inum) == 1){
            inode_t *curr_inode = inodeTable + inum;
            if(offset+nbytes <= curr_inode->size){
                if((offset/UFS_BLOCK_SIZE) < DIRECT_PTRS){
                    if((curr_inode->direct[offset/UFS_BLOCK_SIZE]) != -1){
                        server_message->type = curr_inode->type;
                        int nextBlock = (curr_inode->direct[offset/UFS_BLOCK_SIZE]) * UFS_BLOCK_SIZE + (offset % UFS_BLOCK_SIZE);
                        int rc = pread(file_d, buffer, nbytes, nextBlock);
                        if(rc<0){
                            server_message->returnCode = -1;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

int Creat(int pinum, int type, char *name, msg_t *server_msg)
{
    printf("IN CREAT\n");
    // if invalid pinum or name return -1
    if (pinum > superblock->inode_region_len || pinum < 0 || get_bit(inodeBitmap, pinum) == 0){
        printf("CREAT 1\n");
        printf("pinum: %d\n", pinum);
        printf("superblock->inode_region_len: %d\n", superblock->inode_region_len);
        printf("get_bit(inodeBitmap, pinum): %d\n", get_bit(inodeBitmap, pinum));
        return -1;
    }
    /*
    if (superblock->num_inodes < pinum || pinum < 0 || name == NULL || strlen(name) > 28 || get_bit(inodeBitmap,pinum) == 0)
    {
        printf("CREAT 1\n");
        return -1;
    }
    */

    /*
    // if already created great success!
    if(LookupHelper(pinum, name) != -1){
        server_msg->returnCode = 0;
        return 0;
    }
    */

    inode_t *pinode = inodeTable + pinum;

    if(pinode->type == 1){ //Parent inode is a file 
        printf("CREAT 1\n");
        return -1;
    }

    /*
    // look for spot to put new inode
    int inum = 0;
    for(int i = 0; i < superblock->num_inodes; i++){
        int rel = get_bit(inodeBitmap, i);
        if(rel  == 0){ // found an unallocated inode
            inum = i;
            break;
        }
    }
    */

   int inum = 0;
   for(int i = 0; i < UFS_BLOCK_SIZE / sizeof(int); i++){
        if (get_bit(inodeBitmap,i) == 0){
            inum = i;
            break;
        }
   }

    // unable to allocate a new inode number
    if(inum == -1){
        printf("CREAT 1\n");
        return -1;
    }

    if(type == 0){ // inode is a new directory
        dir_ent_t *parentDir = (fs_img + pinode->direct[0] * UFS_BLOCK_SIZE);
        int inumIdx = 0;
        if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0){
            dot_edge_case = 1;
            inumIdx = 2;
        }
        // iterate until we found empty spot
        while(parentDir[inumIdx].inum != -1){
            /*
            if (parentDir[inumIdx].inum != -1){
                // found matching: Redundant code DELETE!!
            }
            */
            if(strcmp(parentDir[inumIdx].name,name) == 0){
                return 0;
            }
            inumIdx++;
        }

        // allocate new spot in databitmap
        int dnum = -1;
        for (int i = 0; i < (UFS_BLOCK_SIZE/sizeof(int)); i++){ // perhaps look at 1024??
            if (get_bit(dataBitmap,i) == 0){
                dnum = i;
                break;
            }
        }
        // unable to allocate space for it
        if (dnum == -1){
            printf("CREAT 1\n");
            return -1;
        }
        set_bit(dataBitmap,dnum);

        // fill metadata for new inode
        inode_t *inode = inodeTable + inum;
        inode->direct[0] = (superblock->data_region_addr + dnum);
    
        inode->size = 2 * sizeof(dir_ent_t);
        inode->type = type;

        // update parent inode info
        parentDir[inumIdx].inum = inum;
        //(parentDir + inumIdx)->inum = inum;
        strcpy(parentDir[inumIdx].name,name);
        printf("parentDir[inumIdx].name: %s...\n", parentDir[inumIdx].name);
        pinode->size+=sizeof(dir_ent_t);

        // create datablock for parent
        dir_block_t b;
        strcpy(b.entries[0].name, ".");
        printf("b.entries[0].name: %s...\n", b.entries[0].name);
        strcpy(b.entries[1].name, "..");
        printf("b.entries[1].name: %s...\n", b.entries[1].name);
        b.entries[0].inum = inum;
        b.entries[1].inum = pinum;
        for (int k = 2; k < 128; k++){
            b.entries[k].inum = -1;
        }

        // update data region address
        int pwCheck = pwrite(file_d, &b, UFS_BLOCK_SIZE, (superblock->data_region_addr * UFS_BLOCK_SIZE + dnum * UFS_BLOCK_SIZE));
        if (pwCheck < 0){
            printf("CREAT 1\n");
            return -1;
        }
    } else{ // inode is a file
        dir_ent_t *parentDir = (dir_ent_t*)(fs_img + pinode->direct[0] * UFS_BLOCK_SIZE);
        int inumIdx = 0;
        // iterate until we found empty spot
        for(inumIdx = 0; parentDir[inumIdx].inum != -1; inumIdx++){
            if (parentDir[inumIdx].inum != -1){
                // found matching: Redundant code DELETE!!
                if(strcmp(parentDir[inumIdx].name,name) == 0){
                    return 0;
                }
            }
            inumIdx++;
        }
        // mark inum as allocated
        set_bit(inodeBitmap,inum);
        // fill metadata for new inode
        inode_t *inode = inodeTable + inum;
        inode->size = 0;
        inode->type = type;

        for(int i = 1; i < 30; i++){
            inode->direct[i] = -1;
        }

        // update parent inode info
        parentDir[inumIdx].inum = inum;
        //(parentDir + inumIdx)->inum = inum;
        strcpy(parentDir[inumIdx].name,name);
        pinode->size+=32;
    }
    
    int testSync = msync(fs_img,fs_img_size,MS_SYNC);
    if (testSync < 0){
        printf("CREAT 1\n");
        return -1;
    }
    testSync = fsync(file_d);
    if (testSync < 0){
        printf("CREAT 1\n");
        return -1;
    }

    printf("CREAT END\n");
    return 0;
}

int Unlink(int pinum, char *name, msg_t *server_msg)
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
        return -1;
    }
    if((childInum = Lookup(pinum, name, server_msg)) == -1){ 
        return -1;
    }

    inode_t *child = inodeTable + childInum;
    
    if(child->type == 1){ // child is a file
        for(int i = 0; i < DIRECT_PTRS; i++){
            if(child->direct[i] != -1){ //setting direct poitners to zero.
                clear_bit(dataBitmap, child->direct[i]);
            }
        }
    }
    else{ //it is a directory
        if(child->size > 2 * sizeof(msg_t)){ //we have more directory entries than "." and ".."
            return -1;
        }
        /*
        dir_block_t curr_block;
        long curr_addr = (long) (fs_img + superblock->data_region_addr * UFS_BLOCK_SIZE  + child->direct[0] * UFS_BLOCK_SIZE);
        memcpy(&curr_block, (void *)curr_addr, sizeof(dir_block_t));
        if(curr_block.entries[2].inum != -1){
            return -1;
        }
        */
        for(int i = 0; i < DIRECT_PTRS; i++){ //clear child datablocks
            clear_bit(dataBitmap, child->direct[i]);
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

int Shutdown()
{
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
    int port_num = atoi(argv[1]);
    sd = UDP_Open(port_num);
    assert(sd>-1);
    const char *img_path = argv[2];
    file_d = open(img_path, O_RDWR | O_APPEND);

    if (fstat(file_d, &file_info) != 0)
    {
        perror("Fstat failed");
    }

    // TODO: fix parameters
    fs_img = mmap(NULL, (int)file_info.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_d, 0);
    if (fs_img == MAP_FAILED)
    {
        perror("mmap failed");
        exit(1);
    }
    superblock = (super_t *)fs_img;

    inodeTable = (inode_t*)(fs_img + superblock->inode_region_addr * UFS_BLOCK_SIZE);
    inodeBitmap = (unsigned int *)(fs_img + superblock->inode_bitmap_addr * UFS_BLOCK_SIZE);
    dataBitmap = (unsigned int *)(fs_img  + superblock->data_bitmap_addr * UFS_BLOCK_SIZE);
    dataRegion = (void *)(fs_img + superblock->data_region_addr * UFS_BLOCK_SIZE);
    rootInode = inodeTable;
    rootDir = fs_img + (rootInode->direct[0] * UFS_BLOCK_SIZE);
    while (1)
    {
        msg_t message;

        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, (char *)&message, sizeof(msg_t));
        if (rc < 0)
        {
            return -1;
        }
        int r = 0;
        switch (message.func)
        {
        case INIT:
            //Init(message.hostname, message.port, server_message, addr);
            break;
        case LOOKUP:
            r = Lookup(message.pinum, message.name, &message);
            message.returnCode = r;
            UDP_Write(sd, &addr, (char *) &message, sizeof(msg_t));
            break;
        case STAT:
            Stat(message.inum, &message);
            UDP_Write(sd, &addr, (char *) &message, sizeof(msg_t));
            break;
        case WRITE:
            r = Write(message.inum, message.buffer, message.offset, message.nbytes, &message);
            message.returnCode = r;
            UDP_Write(sd, &addr, (char *) &message, sizeof(msg_t));
            break;
        case READ:
            r = Read(message.inum, message.buffer, message.offset, message.nbytes, &message);
            message.returnCode = r;
            UDP_Write(sd, &addr, (char *) &message, sizeof(msg_t));
            break;
        case CREAT:
            r = Creat(message.pinum, message.type, message.name, &message);
            message.returnCode = r;
            UDP_Write(sd, &addr, (char *) &message, sizeof(msg_t));
            break;
        case UNLINK:
            r = Unlink(message.pinum, message.name, &message);
            message.returnCode = r;
            UDP_Write(sd, &addr, (char *) &message, sizeof(msg_t));
            break;
        case SHUTDOWN:
            Shutdown();
            break;
        }
    } 
    return 0;
}
