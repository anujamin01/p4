#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include<signal.h>

#include "udp.h"
#include "ufs.h"
#include "mfs.h"

#include "msg.h"

#define BUFFER_SIZE (4096)
#define BLOCK_SIZE (4096)
#define UFS_DIRECTORY (0)
#define UFS_REGULAR_FILE (1)

typedef struct {
    dir_ent_t entries[128];
} dir_block_t;

int sd, file_d, counter; 
//struct sockaddr_in *sock;
struct sockaddr_in addr;
void *image;
int image_size; 
super_t *sblock;
inode_t *inode_table;
inode_t *root_inode;
dir_ent_t *root_dir;
unsigned int* imap;
unsigned int* dmap; 
char* data_start_ptr;
char* args[4];
char buff[4096]; 


void intHandler(int dummy) {
    UDP_Close(sd);
    exit(130);
}

unsigned int get_bit(unsigned int *bitmap, int position) {
   int index = position / 32;
   int offset = 31 - (position % 32);
   return (bitmap[index] >> offset) & 0x1;
}

void set_bit(unsigned int *bitmap, int position) {
   int index = position / 32;
   int offset = 31 - (position % 32);
   bitmap[index] |= 0x1 << offset;
}

void unset_bit(unsigned int *bitmap, int position) {
   int index = position / 32;
   int offset = 31 - (position % 32);
   bitmap[index] |= 0x0 << offset;
}

unsigned int getUnallocatedBit(unsigned int *bitmap) {
    for (int i = 0; i < 1024; i++) {
        if (get_bit(bitmap, i) == 0) {
            return i; 
        }
    }
    return -1; // all are allocated (need to increase size of bitmap or return -1)
}

int lookupHelper(int pinum, char *name) {
    //check if pinumm < 1 or greater max array
    if(pinum < 0 || get_bit(imap, pinum) == 0){
        return -1;
    }
    
    //get inode of parent   
    inode_t *pinode = inode_table + pinum; 

    if (pinode->type != UFS_DIRECTORY) {
        return -1; 
    }

    //create parentBlock we'll loop through
    dir_ent_t *pBlock = malloc(BLOCK_SIZE);
    
    //loop though the array of data block addresses
    for(int i= 0; i<DIRECT_PTRS; i++){

        if (pinode->direct[i] == -1)
            continue; 

        //the address to the data block
        int addrPointer = pinode->direct[i] * UFS_BLOCK_SIZE; 

        if(addrPointer > 0){
            //move read/write file pointer to address of the specified data block
            int rc = lseek(file_d, addrPointer, SEEK_SET);
            if(rc < 0){
                return -1;
            }
            
            //Read num of bytes (BLOCK_SIZE) into pBlock from fd
            rc = read(file_d, pBlock, BLOCK_SIZE); //IDK IF IT SHOULD BE BLOCK_SIZE???
            //-1 for errors or 0 for EOF
            if(rc < 0){
                return -1;
            }

            //loop through the directories in the data block
            for(int directory = 0; directory < BLOCK_SIZE/sizeof(dir_ent_t); directory++){ 

                dir_ent_t dirEntry = pBlock[directory];

                if(dirEntry.inum==-1){
                    continue;
                }

                if(strcmp(dirEntry.name, name)==0 && dirEntry.inum!=-1){
                    //same names
                    return dirEntry.inum;
                }

            }
        }
    }
    return -1; 
}

int statHelper(int inum, msg_t *m) {
    if(get_bit(imap, inum) == 0)
        return -1; 
   //inode_t *inode = inode_table + (inum * sizeof(inode_t));
   inode_t *inode = inode_table + inum; 
   
   //m->size = inode->size;
   //m->type = inode->type;
   //m->rc = 0; 

   return 0;
}

int writeHelper(int inum, msg_t *m, int offset, int nbytes) {

    if (inum < 1 || get_bit(imap, inum) == 0 || (offset / UFS_BLOCK_SIZE) >= DIRECT_PTRS || (offset+nbytes)> BLOCK_SIZE*DIRECT_PTRS){
        printf("first if statement\n");
        return -1; 
    }
        
   inode_t *inode = inode_table + inum;

    // not regular file or at max size already 
    if(inode->type != MFS_REGULAR_FILE) {
        printf("not reg file stuff \n");
        return -1; 
    }


    // allocate data blocks depending on size of file 
    int sz = inode->size; 
    int bytesWritten; 
    int a; 

    if (sz == 0) { 
        int dblockNum = getUnallocatedBit(dmap);
        if (dblockNum == -1){  //no available spots
            printf("no spots availabe");
            return -1; 
        }
        set_bit(dmap,dblockNum);       
        inode->direct[0] = (sblock->data_region_addr + dblockNum);
        a = inode->direct[0] * UFS_BLOCK_SIZE; 
        bytesWritten = pwrite(file_d, m->buffer, nbytes, a);
    } else { 
        int idx = offset / UFS_BLOCK_SIZE; 
        if (idx >= DIRECT_PTRS){ //max size
            printf("checking index\n");
            return -1;  //TODO when I change this to a diff number it returns that number but when I change it back to -1 it doesnt timesout???
        }
        int blockNum = inode->direct[offset / UFS_BLOCK_SIZE];

        if (blockNum == -1){
            int dblockNum = getUnallocatedBit(dmap);
            if (dblockNum == -1) { //no available spots
                printf("no spots");
                return -1; 
            }
            set_bit(dmap,dblockNum);       
            inode->direct[idx] = (sblock->data_region_addr + dblockNum);
        }

        int offsetIntoBlock = offset % UFS_BLOCK_SIZE;

        a = inode->direct[idx] * UFS_BLOCK_SIZE + offsetIntoBlock;
        // a = inode->direct[idx] * UFS_BLOCK_SIZE;
        bytesWritten = pwrite(file_d, m->buffer, nbytes, a);
     }

    // // // writes nbyte bytes from buf to the fd. The offset value defines the starting position in the file
    // // int bytes = pwrite(fd, buffer, nbytes, offset);
    
    //pwrite should return the number of bytes written of -1 if it wasn't successful
    if(bytesWritten != nbytes || bytesWritten == -1){
        printf("In the if statement\n");
        return -1; //TODO CLIENT:: sent message WRITE with inum: 1, offset: 40960 and nbytes: 4096 failing here for maxfile test
    }

    inode->size = offset + nbytes;

    int rc = msync(image, image_size, MS_SYNC); // or fsync: info related to fd
    assert(rc > -1);
    rc = fsync(file_d);
    assert(rc > -1);
    return 0;  
}

void readHelper(int inum, msg_t  m, int offset, int nbytes) {

    int ret = -1; 
    //check if pinumm < 1 or greater max array
    if(inum >= 0 && get_bit(imap,inum) == 1)  {

        inode_t *inode = inode_table + inum;

        if (offset + nbytes <= inode->size && (offset / UFS_BLOCK_SIZE) < DIRECT_PTRS) {
        
            int blockNum = inode->direct[offset / UFS_BLOCK_SIZE];
            if (blockNum != -1) {

                int offsetIntoBlock = offset % UFS_BLOCK_SIZE; 
                m.type = inode->type; 
                    
                int a = blockNum * UFS_BLOCK_SIZE + offsetIntoBlock;
                int rc =  pread(file_d, m.buffer, nbytes, a);
                if(rc < 0)
                    ret = -1; 
                
                ret = 0; 
            }
        }
    }
    //m.returnCode = ret; 
    UDP_Write(sd, &addr, (char *) &m, sizeof(msg_t));
}

//TODO if we create a new inode and write to the parent inode to we need to pwrite??
int creatHelper(int pinum, int type, char *name) {

    //check if pinumm < 1 or greater max array
    if(pinum > sblock->inode_region_len || pinum < 0 || get_bit(imap, pinum) == 0){
        return -1;
    }

    //inode_t *pinode = inode_table + (pinum * sizeof(inode_t));
    inode_t *pinode = inode_table + pinum; 

    if (pinode->type != UFS_DIRECTORY)
        return -1; 

    if (type == UFS_REGULAR_FILE) {
        // check if file already exists
        dir_ent_t *dir = image + (pinode->direct[0] * UFS_BLOCK_SIZE);
        int i = 0;
        while(dir[i].inum != -1) {
            if(strcmp(name, dir[i].name) == 0) {
                return 0;
            }
            i++;
        }

        //find unallocated inode and set as allocated 
        int inum = getUnallocatedBit(imap); 
        if (inum == -1) { //no available spots
            return -1; 
        }
        set_bit(imap, inum); 
        inode_t *inode = inode_table + inum;
        inode->type = type;
        inode->size = 0;


        // write to parent
        dir[i].inum = inum;
        strcpy(dir[i].name, name); //TODO change from strcpy
        pinode->size += sizeof(dir_ent_t);

    } else {  //MFS_DIRECTORY
        // check if file already exists
        dir_ent_t *dir = image + (pinode->direct[0] * UFS_BLOCK_SIZE);
        int i = 0;
        while(dir[i].inum != -1) {
            if(strcmp(name, dir[i].name) == 0) {
                return 0;
            }
            i++;
        }

        int dblockNum = getUnallocatedBit(dmap);
        if (dblockNum == -1)  //no available spots
            return -1; 
        set_bit(dmap,dblockNum);

        //find unallocated inode and set as allocated 
        int inum = getUnallocatedBit(imap); 
        if (inum == -1) { //no available spots
            return -1; 
        }
        set_bit(imap, inum); 

        //inode_t *inode = inode_table + (inum * sizeof(inode_t));
        inode_t *inode = inode_table + inum; 
        inode->type = type;
        inode->size = 2 * sizeof(dir_ent_t);
        inode->direct[0] = (sblock->data_region_addr + dblockNum);  

        // write to parent
        dir[i].inum = inum;
        strcpy(dir[i].name, name);
        pinode->size += sizeof(dir_ent_t);

        dir_block_t dblock;
        strcpy(dblock.entries[0].name, "."); //TODO change from strcopy
        dblock.entries[0].inum = inum;

        strcpy(dblock.entries[1].name, "..");
        dblock.entries[1].inum = pinum;

        for (int j = 2; j < 128; j++)
            dblock.entries[j].inum = -1;
        
        if (pwrite(file_d, &dblock, UFS_BLOCK_SIZE, (sblock->data_region_addr + dblockNum) * UFS_BLOCK_SIZE) < 0)
            return -1;
    }   
     int rc = msync(image, image_size, MS_SYNC); 
     assert(rc > -1);
     rc = fsync(file_d);
    assert(rc > -1); 
    return 0;
}

int unlinkHelper(int pinum, char *name) {

    int ret = -1;
    if (get_bit(imap, pinum) == 0) {
        return -1;
    }

    inode_t *pinode = inode_table + pinum;

    // find corresponding inode 
    int inum;
    for(int i = 0; i < DIRECT_PTRS; i++) {
        int blockNum = pinode->direct[i];
        if (blockNum == -1) {
            break;
        }
        dir_block_t *dir = (dir_block_t*)(image + (blockNum * UFS_BLOCK_SIZE));
        for (int j = 0; j < 128; j++) {
            if(dir->entries[j].inum != 0)
            if((strcmp(dir->entries[j].name, name) == 0) && dir->entries[j].inum != -1) {
                inum = dir->entries[j].inum;
                break;
            }
        }
    }

    inode_t *inode = inode_table + inum;

    if (inode->type == MFS_DIRECTORY) { 
        // make sure directory is empty
        if (inode->size == 2*sizeof(dir_ent_t)) {

            unset_bit(dmap, inum);
            unset_bit(imap, inum);
            pinode->size -= sizeof(dir_ent_t);

            // update parent directory
            dir_ent_t *parent_dir = image + (pinode->direct[0] * UFS_BLOCK_SIZE);
            for (int i = 0; i < 128; i++) {
                if (parent_dir[i].inum == inum) {
                    parent_dir[i].inum = -1;
                    ret = 0;
                    break;
                }
            }
        }
    } else { 
        unset_bit(dmap, inum);
        unset_bit(imap, inum);
        pinode->size -= sizeof(dir_ent_t);

        // update parent directory
        dir_ent_t *parent_dir = image + (pinode->direct[0] * UFS_BLOCK_SIZE);
        for (int i = 0; i < 128; i++) {
            if (parent_dir[i].inum == inum) {
                parent_dir[i].inum = -1;
                ret = 0;
                break;
            }
        }
    }
    
    int rc = msync(image, image_size, MS_SYNC); // or fsync: info related to fd
    assert(rc > -1);
    rc = fsync(file_d);
    assert(rc > -1);
    return ret;
}

void shutdownHelper() {
    int rc = fsync(file_d);
    assert(rc > -1); 
	close(file_d);
	exit(0);
}

//server code
int main(int argc, char *argv[]) {

    signal(SIGINT, intHandler);

    // create socket and bind it to port 
    int portnum = atoi(argv[1]);
    sd = UDP_Open(portnum);
    assert(sd > -1);

    counter = 0; 

    char *f = argv[2];
    file_d = open(f, O_RDWR);
    assert(file_d > -1);

    struct stat sbuf;
    int rc = fstat(file_d, &sbuf);
    assert(rc > -1);

    image_size = (int) sbuf.st_size;

    image = mmap(NULL, image_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_d, 0);
    assert(image != MAP_FAILED);

    sblock = (super_t *)image;

    inode_table = image + (sblock->inode_region_addr * UFS_BLOCK_SIZE);
    root_inode = inode_table;
    root_dir = image + (root_inode->direct[0] * UFS_BLOCK_SIZE);
    imap = image + (sblock->inode_bitmap_addr * UFS_BLOCK_SIZE); 
    dmap = image + (sblock->data_bitmap_addr * UFS_BLOCK_SIZE); 
    data_start_ptr = image + (sblock->data_region_addr * UFS_BLOCK_SIZE); 
     
    while (1) {
        msg_t message;

        printf("server:: waiting...\n");
        int rc = UDP_Read(sd, &addr, (char *) &message, sizeof(message));
        printf("IN SERVER\n");
    
        if (rc > 0) {
            int ret = 0; 
            switch(message.func) {
                case LOOKUP:
                    //lookup
                     ret = lookupHelper(message.pinum, message.name);
                     rc = UDP_Write(sd, &addr, (char *) &ret, sizeof(ret));
                    break;
                case STAT:
                    //stat
                    ret = statHelper(message.inum, &message); 
                    rc = UDP_Write(sd, &addr, (char *) &message, sizeof(msg_t));
                    break; 
                case WRITE:
                    //write
                    ret = writeHelper(message.inum, &message, message.offset, message.nbytes);
                    rc = UDP_Write(sd, &addr, (char *) &ret, sizeof(ret));
                    break;
                case READ:
                    //read
                    readHelper(message.inum, message, message.offset, message.nbytes);
                    break;
                case CREAT:
                    //create
                    ret = creatHelper(message.pinum, message.type, message.name);
                    rc = UDP_Write(sd, &addr, (char *) &ret, sizeof(ret));
                    break; 
                case UNLINK:
                    //unlink
                    ret = unlinkHelper(message.pinum, message.name); 
                    rc = UDP_Write(sd, &addr, (char *) &ret, sizeof(ret));
                    break; 
                case SHUTDOWN:
                    //shutdown
                    shutdownHelper(); 
                    break; 
            }

            //TODO update counter
            printf("server:: reply\n");
        }
    }
    return 0;
}
