#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <unordered_map>
#include "Tiny_disk.h"
#include "Tiny_buf.h"

using namespace std;

class BufferManager 
{
public:
    static const int NBUF = 30;          // Number of buffers
    static const int BUFFER_SIZE = 512;  // Size of each buffer

private:
    Buf* bFreeList = new Buf;            // Free buffer list head
    Buf m_Buf[NBUF];                     // Array of buffer headers
    unsigned char Buffer[NBUF][BUFFER_SIZE]; // Array of actual buffer storage
    unordered_map<int, Buf*> map;        // Map to associate block numbers with buffer headers

public:
    // Constructor
    BufferManager();
    
    // Destructor
    ~BufferManager();

    // Initialize the buffer manager
    void Initialize();

    // Get a buffer for a specific block number
    Buf* GetBlk(int blkno);

    // Release a buffer
    void Brelse(Buf* bp);

    // Read a block into a buffer
    Buf* Bread(int blkno);

    // Write a buffer to disk
    void Bwrite(Buf* bp);

    // Write a buffer to disk asynchronously
    void Bdwrite(Buf* bp);

    // Clear a buffer
    void Bclear(Buf* bp);

    // Flush all buffers to disk
    void Bflush();

    // Format the buffer manager
    void Format();

private:
    // Detach a buffer from the free list
    void detach(Buf* bp);

    // Insert a buffer into the free list
    void insert(Buf* bp);
};