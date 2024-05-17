#include "Tiny_fileSystem.h"
#include "Tiny_user.h"
#include "Tiny_disk.h"
#include "Tiny_openFileManager.h"
#include "Tiny_cache.h"
#include "Tiny_buf.h"
#include "Tiny_Superblock.h"
#include "Tiny_DiskNode.h"
extern User myUser;
extern Disk myDisk;
extern BufferManager myBufferManager;
extern SuperBlock mySuperBlock;
extern InodeTable myInodeTable;

FileSystem::FileSystem()
{
    if (!myDisk.Exists())
		FormatDevice();
	else
		LoadSuperBlock();
}

FileSystem::~FileSystem()
{
    Update();
}

void FileSystem::FormatDevice() 
{
    // Format the SuperBlock
    mySuperBlock.s_isize = FileSystem::INODE_SECTOR_NUMBER;
    mySuperBlock.s_fsize = FileSystem::DISK_SECTOR_NUMBER;
    mySuperBlock.s_nfree = 0;
    mySuperBlock.s_free[0] = -1;  // Initialize with -1 as the end marker
    mySuperBlock.s_ninode = 0;
    mySuperBlock.s_fmod = 0;
    time((time_t*)&mySuperBlock.s_time);
    
    myDisk.Construct();
    myDisk.write((unsigned char*)(&mySuperBlock), sizeof(SuperBlock), 0);

    // Initialize and write root inode and empty inodes
    DiskInode emptyDINode, rootDINode;
    rootDINode.d_mode |= Inode::IALLOC | Inode::IFDIR;  // Set root inode as allocated and directory
    myDisk.write((unsigned char*)&rootDINode, sizeof(rootDINode), FileSystem::INODE_ZONE_START_SECTOR);
    for (int i = 1; i < FileSystem::INODE_NUMBER_ALL; ++i) {
        if (mySuperBlock.s_ninode < 100)
            mySuperBlock.s_inode[mySuperBlock.s_ninode++] = i;
        myDisk.write((unsigned char*)&emptyDINode, sizeof(emptyDINode), FileSystem::INODE_ZONE_START_SECTOR + i);
    }

    // Format data sectors
    char freeBlock[BLOCK_SIZE], freeBlock1[BLOCK_SIZE];
    memset(freeBlock, 0, BLOCK_SIZE);
    memset(freeBlock1, 0, BLOCK_SIZE);

    for (int i = 0; i < FileSystem::DATA_SECTOR_NUMBER; ++i) {
        if (mySuperBlock.s_nfree >= 100) {
            memcpy(freeBlock1, &mySuperBlock.s_nfree, sizeof(int) + sizeof(mySuperBlock.s_free));
            myDisk.write((unsigned char*)&freeBlock1, BLOCK_SIZE, FileSystem::DATA_START_SECTOR + i);
            mySuperBlock.s_nfree = 0;
        }
        else
            myDisk.write((unsigned char*)freeBlock, BLOCK_SIZE, FileSystem::DATA_START_SECTOR + i);
        mySuperBlock.s_free[mySuperBlock.s_nfree++] = i + FileSystem::DATA_START_SECTOR;
    }

    // Update and write the SuperBlock again to reflect changes
    time((time_t*)&mySuperBlock.s_time);
    myDisk.write((unsigned char*)(&mySuperBlock), sizeof(SuperBlock), 0);
}


void FileSystem::LoadSuperBlock()
{
	fseek(myDisk.diskPointer, 0, SEEK_SET);
	myDisk.read((unsigned char *)(&mySuperBlock), sizeof(SuperBlock), 0);
}

void FileSystem::Update()
{
    Buf* pBuf;
    mySuperBlock.s_fmod = 0;  // Mark the superblock as not modified
    mySuperBlock.s_time = (int)time(NULL);  // Update the superblock's timestamp

    // Write the superblock to disk, handling it in two parts due to its size
    for (int j = 0; j < 2; j++) {
        pBuf = myBufferManager.GetBlk(FileSystem::SUPERBLOCK_START_SECTOR + j);  // Get block for superblock part
        if (!pBuf) {
            std::cerr << "Failed to get block for superblock update." << std::endl;
            continue;
        }
        int* p = (int*)&mySuperBlock + j * 128;  // Calculate the pointer offset for each part
        memcpy(pBuf->b_addr, p, BLOCK_SIZE);  // Copy superblock data to the buffer
        myBufferManager.Bwrite(pBuf);  // Write the buffer to disk
    }

    myInodeTable.UpdateInodeTable();  // Update the inode table
    myBufferManager.Bflush();  // Flush all buffered changes to disk
}

Buf* FileSystem::Alloc()
{
    // Start of function
    int blkno;
    Buf* pBuf;
    // Check if there are any free blocks
    if (mySuperBlock.s_nfree <= 0) {
        // No free blocks
        myUser.u_error = User::U_ENOSPC;
        return NULL;
    }

    // Get the next free block number
    blkno = mySuperBlock.s_free[--mySuperBlock.s_nfree];
    // If the block number is negative, we have a problem
    if(blkno <= 0) {
        // No free blocks
        mySuperBlock.s_nfree = 0;
        myUser.u_error = User::U_ENOSPC;
        return NULL;
    }

    // If we have reached the end of the free list, we need to read the free block list from disk
    if(mySuperBlock.s_nfree == 0) {
        // Read the free block list from disk
        pBuf = myBufferManager.Bread(blkno);
        // Get the number of free blocks from the first int in the buffer
        int* p = (int *)pBuf->b_addr;
        mySuperBlock.s_nfree = *p++;
        // Copy the free block numbers from the buffer to the superblock
        memcpy(mySuperBlock.s_free, p, sizeof(mySuperBlock.s_free));
        // Release the buffer
        myBufferManager.Brelse(pBuf);
    }

    // Get the buffer for the block number
    pBuf = myBufferManager.GetBlk(blkno);
    // If the buffer is not NULL, clear it
    if (pBuf) {
        myBufferManager.Bclear(pBuf);
        // Set the modified flag
        mySuperBlock.s_fmod = 1;
    }

    // Return the buffer
    return pBuf;
}


void FileSystem::Free(int blkno)
{
    Buf* pBuf;
    // Check if there were previously no free blocks in the system
    if (mySuperBlock.s_nfree <= 0) {
        mySuperBlock.s_nfree = 1;
        mySuperBlock.s_free[0] = 0; // Use 0 as a marker to indicate the end of the free block chain
    }

    // If the stack for free disk block numbers directly managed by the SuperBlock is full
    if (mySuperBlock.s_nfree >= 100) {
        // Use the block currently being freed to store the index table for the previous set of 100 free blocks
        pBuf = myBufferManager.GetBlk(blkno); // Allocate buffer for the block being freed

        // Start recording from byte 0 of this disk block, occupying 4 (s_nfree) + 400 (s_free[100]) bytes in total
        int* p = (int *)pBuf->b_addr;
        
        // First write the number of free blocks
        *p++ = mySuperBlock.s_nfree;
        memcpy(p, mySuperBlock.s_free, sizeof(int) * 100);
        mySuperBlock.s_nfree = 0;

        // Write the block containing the free block index table back to the disk
        myBufferManager.Bwrite(pBuf);
    }

    // Record the currently freed block number in the SuperBlock
    mySuperBlock.s_free[mySuperBlock.s_nfree++] = blkno;
    mySuperBlock.s_fmod = 1; // Mark the SuperBlock as modified
}

Inode* FileSystem::IAlloc()
{
    Buf* pBuf;
    Inode* pNode;
    int ino;
    // Check if there are no free inodes available in the memory cache
    if(mySuperBlock.s_ninode <= 0) {
        ino = -1;
        // Loop through inode zones on disk
        for(int i = 0; i < mySuperBlock.s_isize; i++) {
            pBuf = myBufferManager.Bread(FileSystem::INODE_ZONE_START_SECTOR + i);
            int* p = (int *)pBuf->b_addr;
            // Scan each inode in the sector
            for(int j = 0; j < FileSystem::INODE_NUMBER_PER_SECTOR; j++){
                ino++;
                int mode = *( p + j * sizeof(DiskInode)/sizeof(int) );
                // Skip if inode is not free
                if(mode) continue;
                // Check if the inode is not loaded into memory
                if(myInodeTable.IsLoaded(ino) == -1 ) {
                    mySuperBlock.s_inode[mySuperBlock.s_ninode++] = ino;
                    // Stop if the free inode table is full
                    if(mySuperBlock.s_ninode >= 100) {
                        break;
                    }
                }
            }
            myBufferManager.Brelse(pBuf);
            // Break the outer loop if the table is full
            if(mySuperBlock.s_ninode >= 100) {
                break;
            }
        }
        // Return NULL if no free inodes are found
        if(mySuperBlock.s_ninode <= 0){
            myUser.u_error = User::U_ENOSPC;
            return NULL;
        }
    }
    // Retrieve the next free inode from the cache
    ino = mySuperBlock.s_inode[--mySuperBlock.s_ninode];
    pNode = myInodeTable.IGet(ino);
    if(NULL == pNode) {
        cout << "No free memory to allocate INODE!" << endl;
        return NULL;
    }
    // Clear inode data for reuse
    pNode->Clean();
    // Mark superblock as modified
    mySuperBlock.s_fmod = 1;
    return pNode;
}


void FileSystem::IFree(int number)
{
    // Check if the number of free inodes managed by the superblock has reached the limit
    if(mySuperBlock.s_ninode >= 100) {
        // Instead of ignoring the request, scatter the freed inode across the disk inode area
        return;
    }
    // Add the inode number back to the superblock's free inode list
    mySuperBlock.s_inode[mySuperBlock.s_ninode++] = number;

    // Set the superblock modified flag
    mySuperBlock.s_fmod = 1;
}