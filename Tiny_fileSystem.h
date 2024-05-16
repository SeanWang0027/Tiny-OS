#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include "Tiny_inode.h"
#include "Tiny_cache.h"
#include "Tiny_buf.h"
#include "Tiny_disk.h"
#include "Tiny_DiskNode.h"

class FileSystem 
{
public:
    static const int BLOCK_SIZE = 512; // Size of a block in bytes
    static const int DISK_SECTOR_NUMBER = 16384; // Total number of sectors on the disk
    static const int SUPERBLOCK_START_SECTOR = 0; // Starting sector of the superblock

    static const int INODE_ZONE_START_SECTOR = 2; // Starting sector of the Inode zone
    static const int INODE_SECTOR_NUMBER = 1022; // Number of sectors occupied by the Inode zone
    static const int INODE_SIZE = sizeof(DiskInode); // Size of an Inode

    static const int INODE_NUMBER_PER_SECTOR = BLOCK_SIZE / INODE_SIZE; // Number of Inodes per sector
    static const int ROOT_INODE_NO = 0; // Root Inode number
    static const int INODE_NUMBER_ALL = INODE_SECTOR_NUMBER * INODE_NUMBER_PER_SECTOR; // Total number of Inodes

    static const int DATA_START_SECTOR = INODE_ZONE_START_SECTOR + INODE_SECTOR_NUMBER; // Starting sector of the data area
    static const int DATA_END_SECTOR = DISK_SECTOR_NUMBER - 1; // Ending sector of the data area
    static const int DATA_SECTOR_NUMBER = DISK_SECTOR_NUMBER - DATA_START_SECTOR; // Number of sectors in the data area

    FileSystem();
    ~FileSystem();

    void FormatDevice(); // Format the device

    void LoadSuperBlock(); // Load the superblock into memory
    void Update(); // Update the file system metadata

    Inode* IAlloc(); // Allocate an Inode
    void IFree(int number); // Free an Inode

    Buf* Alloc(); // Allocate a block
    void Free(int blkno); // Free a block
};
