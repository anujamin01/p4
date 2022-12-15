void create_helper(client_message message, struct sockaddr_in addr) {
    printf("Create\n");
    server_message response;
    int pinum = message.pinum;
    int type = message.type;
    
    if(check_inode(pinum) || inodes[pinum].type == 1){ //if it is invalid, not used, or not a directory, return an error
        response.returnCode = -1;
    }
    else{
        //see if name already exists first
        dir_block_t *dir_entries;
        int alreadyExists = 0;
        
        for(int i = 0; i < 30; i++){
            if(inodes[pinum].direct[i] != -1){
                dir_entries = (dir_block_t *) (image_start + (inodes[pinum].direct[i] * 4096));
                for(int j = 0; j < 128; j++){
                    if (dir_entries->entries[j].inum != -1) {
                        if(strcmp(dir_entries->entries[j].name, message.name) == 0){
                            response.returnCode = 0;
                            alreadyExists = 1;
                            break;
                        }
                    }
                }
            }
        }

        if(alreadyExists == 0){
            if(inodes[pinum].size % 4096 == 0){ //entire first data block is full, allocate new data block
                //traverse data bitmap to find an unallocated data block
                //that unallocated data block index equal to direct[inodes[pinum].size / 4096]
                for(int i = 0; i < superblock->data_bitmap_len * 4096; i++){
                    if(get_bit((unsigned int *)(image_start + superblock->data_bitmap_addr*4096), i) ==  0){
                        set_bit((unsigned int *)(image_start + superblock->data_bitmap_addr*4096), i);
                        inodes[pinum].direct[inodes[pinum].size / 4096] =  superblock->data_region_addr + i;
                        dir_entries = (dir_block_t *) image_start + (inodes[pinum].direct[inodes[pinum].size / 4096] * 4096);
                        memcpy(dir_entries->entries[0].name, message.name, strlen(message.name) + 1);
                        //traverse the inode bitmap and find an unallocated inode
                        //record this index
                        for(int j = 0; j < superblock->inode_bitmap_len * 4096; j++){
                            if(get_bit((unsigned int *)(image_start + 4096), j) == 0){
                                set_bit((unsigned int *)(image_start + 4096), j);
                                dir_entries->entries[0].inum = j;
                                inodes[j].type = type;
                                for(int z = 0; z < 30; z++){
                                    inodes[j].direct[z] = -1;
                                }
                                
                                if(type == 0){
                                    for(int w = 0; w < superblock->data_bitmap_len * 4096; w++){
                                        if(get_bit((unsigned int *)(image_start + superblock->data_bitmap_addr*4096), w) ==  0){
                                            set_bit((unsigned int *)(image_start + superblock->data_bitmap_addr*4096), w);
                                            
                                            msync(image_start, fsize, MS_SYNC);
                                            inodes[j].direct[0] = superblock->data_region_addr + w;
                                            inodes[j].size = 64;
                                            //add the two entries to the new directory
                                            dir_block_t *new_dir_entries;
                                            new_dir_entries = (dir_block_t *) (image_start + inodes[j].direct[0] * 4096);
                                            memcpy(new_dir_entries[0].entries->name, ".", 2);
                                            memcpy(new_dir_entries[1].entries->name, "..", 3);
                                            new_dir_entries[0].entries->inum = j;
                                            new_dir_entries[1].entries->inum = pinum;
                                            break;
                                        }
                                    }
                                }
                                break;
                            }
                        }
                        
                        response.returnCode = 0;
                        inodes[pinum].size += 32;
                        msync(image_start, fsize, MS_SYNC);
                        break;
                    }
                }
            }
            else{
                for(int i = 0; i < 30; i++){
                    if(inodes[pinum].direct[i] != -1){
                        dir_entries = (dir_block_t *) (image_start + (inodes[pinum].direct[i] * 4096));
                        for(int j = 0; j < 128; j++){
                            if(dir_entries->entries[j].inum == -1){ //find a directory entry that is not in use
                                memcpy(dir_entries->entries[j].name, message.name,  strlen(message.name) + 1);
                                //traverse the inode bitmap and find an unallocated inode
                                //record this index
                                msync(image_start, fsize, MS_SYNC);
                                for(int y = 0; y < superblock->inode_bitmap_len * 4096; y++){
                                    if(get_bit(inode_bitmap, y) == 0){
                                        
                                        set_bit((unsigned int *)(image_start + 4096), y);
                                        dir_entries->entries[j].inum = y;
                                        inodes[y].type = type;
                                        msync(image_start, fsize, MS_SYNC);
                                        for(int z = 0; z < 30; z++){
                                            inodes[y].direct[z] = -1;
                                        }
                                    
                                        if(type == 0){
                                            for(int w = 0; w < superblock->data_bitmap_len * 4096; w++){
                                                if(get_bit((unsigned int *)(image_start + superblock->data_bitmap_addr*4096), w) ==  0){
                                                    set_bit((unsigned int *)(image_start + superblock->data_bitmap_addr*4096), w);
                                                    inodes[y].direct[0] = superblock->data_region_addr + w;
                                                    inodes[y].size = 64;
                                                    //add the two entries to the new directory
                                                    dir_block_t *new_dir_entries;
                                                    new_dir_entries = (dir_block_t *) (image_start + inodes[y].direct[0] * 4096);
                                                    memcpy(new_dir_entries[0].entries->name, ".", 1);
                                                    memcpy(new_dir_entries[1].entries->name, "..", 2);
                                                    new_dir_entries[0].entries->inum = y;
                                                    new_dir_entries[1].entries->inum = pinum;
                                                    for(int l = 2; l < 128; l++){
                                                        new_dir_entries->entries[l].inum = -1;
                                                    }

                                                    break;
                                                }
                                            }
                                        }
                                        break;
                                    }
                                    //what if it cant find an inode in the inode bitmap?
                                }
                                inodes[pinum].size += 32;
                                response.returnCode = 0;
                                msync(image_start, fsize, MS_SYNC);
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }
    UDP_Write(sd, &addr, (char*)&response, sizeof(response));
}