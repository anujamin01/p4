int Creat(int pinum, int type, char *name, s_msg_t server_msg, struct sockaddr_in addr)
{
    // if invalid pnum or invalid/too long of name
    if (pinum < 0 || name == NULL || superblock->num_inodes < pinum || strlen(name) > BUFFER_SIZE)
    {
        return -1;
    }

    super_t s = *superblock;
    // if pinum unallocated 
    if(checkInodeAllocated(pinum) == 0){
        return -1;
    }
    // if already created great success!
    if(Lookup(pinum, name) != -1){
        return 0;
    }

    long address = (long)(fs_img + s.inode_region_addr * UFS_BLOCK_SIZE + (pinum * sizeof(inode_t)));
    inode_t pinode;
    memcpy(&pinode, (void*)address, sizeof(inode_t));

    // 1. Pull inode bitmap
    unsigned int bits[UFS_BLOCK_SIZE / sizeof(unsigned int)];
    memcpy(bits, fs_img + s.inode_bitmap_addr * UFS_BLOCK_SIZE, UFS_BLOCK_SIZE);
    
    int inode_num = -1;
    // int inode_idx
    // find first unallocated inode

    //2. Find first available spot in inode table by iterating through bitmap
    for(int i = 0; i < s.num_inodes; i++){
        if(get_bit((unsigned int *)bits, i) == 0){
            inode_num = i;

            // 3. Mark that bit as a 1, indicating we are now putting an inode in that spot
            set_bit((unsigned int*) bits, inode_num);
            break;
        }
    }

    // 4. Create an inode struct, fill it with relevant info
    inode_t inode;
    inode.size = 0;
    inode.type = type;

    // unable to allocate a new inode number
    if(inode_num == -1){
        return -1;
    }

    //Write out the set inode bitmap to disk
    pwrite(file_d, &bits, UFS_BLOCK_SIZE, UFS_BLOCK_SIZE * s.inode_bitmap_addr);

    // num of dir entries in the pinode
    int num_dir_ent = (pinode.size) / sizeof(dir_ent_t);

    // Writing to parent inode's directory entries, if space already exists
    int spaceFound = 0;
    for (int j = 0; j < DIRECT_PTRS; j++){
        if (pinode.direct[j] != -1){
            long curr_data_region = (long)(fs_img + (pinode.direct[j] * UFS_BLOCK_SIZE));

            dir_block_t db;
            memcpy(&db, (void*)curr_data_region, UFS_BLOCK_SIZE);

            for (int k = 0; k < num_dir_ent; k++){
                if(db.entries[k].inum == -1){
                    db.entries[k].inum = inode_num;
                    strcpy(db.entries[k].name,name);
                    pwrite(file_d, &db, sizeof(dir_block_t), curr_data_region);
                    spaceFound = 1;
                    break;
                }
            }   
        }
    }
    //TODO: test to see if it is actually written :)

    //Allocating a new data block to parent directory inode if the existing data blocks are full
    if(!spaceFound){
        // if we get here, wesize need to allocate a new block

       //5. Pull datablock bitmap
        unsigned int data_bits[UFS_BLOCK_SIZE / sizeof(unsigned int)];
        memcpy(data_bits, fs_img + s.data_bitmap_addr * UFS_BLOCK_SIZE, UFS_BLOCK_SIZE);

        for (int l = 0; l < DIRECT_PTRS; l++){ // num_data_blocks
            if (pinode.direct[l] == -1){
                // allocate this block
                int data_num = -1;
                // find first unallocated data
                for(int n = 0; n < s.num_data; n++){
                    if(get_bit((unsigned int *)data_bits, n) == 0){ //!Allocated
                        dir_block_t db;
                        db.entries[0].inum = inode_num; //first entry is new dir_ent_t
                        strcpy(db.entries[0].name, name);
                        pinode.direct[l] = n + s.data_region_addr; //Current index in data bitmap + start of data region
                        for(int i = 1; i < 128; i++){
                            db.entries[i].inum = -1;
                        }

                        // Re-write out data bitmap done here, I think
                        set_bit((unsigned int*)data_bits, n);

                        //TODO: Re-write out data bitmap, directory inode, and new datablock to disk.

                        
                        /*
                            1. Pull inode bitmap
                            2. Find first available spot in inode table by iterating through bitmap
                            3. Mark that bit as a 1, indicating we are now putting an inode in that spot
                            4. Create an inode struct, fill it with relevant info
                            5. Pull datablock bitmap
                            6. Find first available data block by iteration
                            7. Mark that spot in the bitmap as being filled
                            8. Put the address of that data block in your new inode's direct pointer list
                            9. memcpy() (or something else idk) your inode into the inode table
                               at the index you found in step 2
                        */


                        // TODO: not s.data_region_addr, what is it?
                        long curr_data_region = (long)(fs_img + (s.data_region_addr * UFS_BLOCK_SIZE));


                        // Re-write new datablock to disk
                        pwrite(file_d, &db, sizeof(dir_block_t), curr_data_region);  

                    }
                }
            }
        }
    }

    // Re-write directory inode

    // Finally, we write out our new inode

    //long new_address = (long)(fs_img + s.inode_region_addr * UFS_BLOCK_SIZE + (inode_num * sizeof(inode_t)));
    inode_t new_inode_t;
    //memcpy(&new_inode_t, (void*)new_address, sizeof(inode_t));

    new_inode_t.type = type;

    // check if it is dir. if so, fill with . and ..
    dir_block_t curr_db;
    //memcpy(&curr_db, )
    if (type == MFS_DIRECTORY){
        new_inode_t.size = 2 * sizeof(dir_ent_t);
        
        strcpy(curr_db.entries[0].name, ".");
        curr_db.entries[0].inum = inode_num;

        strcpy(curr_db.entries[1].name, "..");
        curr_db.entries[1].inum = pinum;

        for (int i = 2; i < 128; i++)
	    curr_db.entries[i].inum = -1;
    }

    else{ // singular file
        //new_inode.size = 0;


        for (int i = 0; i < 128; i++)
	    curr_db.entries[i].inum = -1;
    }

    // Then something like this?:
    // TODO: not s.data_region_addr, what is it?
    long curr_data_region = (long)(fs_img + s.inode_region_addr + (inode_num *sizeof(inode_t))); // index times size(inote_t)))//(s.data_region_addr * UFS_BLOCK_SIZE));
    //pwrite(file_d, &)
    //memcpy()
    //pwrite(file_d, &curr_db, sizeof(dir_block_t), curr_data_region);  

    fsync(file_d);
    return -1;
}