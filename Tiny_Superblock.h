#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include "Tiny_inode.h"
#include "Tiny_cache.h"
#include "Tiny_buf.h"
#include "Tiny_disk.h"

class SuperBlock 
{
public:
    const static int FREE_SIZE = 100;
    const static int INODE_SIZE = 100;
    const static int PADDING_SIZE = 47;

public:
    int s_isize;            // Number of blocks occupied by the external Inode area
    int s_fsize;            // Total number of data blocks in the file system

    int s_nfree;            // Number of free blocks directly managed
    int s_free[FREE_SIZE];        // Index table of directly managed free blocks
    int s_flock;            // Lock for the free block index table (unused, can be deleted)

    int s_ninode;           // Number of free external Inodes directly managed
    int s_inode[INODE_SIZE];       // Index table of directly managed free external Inodes
    int s_ilock;            // Lock for the free Inode table

    int s_fmod;             // Modification flag for the in-memory super block copy, indicating the need to update the corresponding super block on external storage
    int s_ronly;            // Indicates the file system is read-only
    int s_time;             // Last update time
    int padding[PADDING_SIZE];        // Padding to make the SuperBlock size equal to 1024 bytes, occupying 2 sectors
};
