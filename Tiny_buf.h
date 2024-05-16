#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <unordered_map>
#include "Tiny_disk.h"

using namespace std;

class Buf
{
public:
    // Enumeration for buffer flags
    enum BufFlag
    {
        B_WRITE = 0x1,   // Buffer is being written to disk
        B_READ  = 0x2,   // Buffer is being read from disk
        B_DONE  = 0x4,   // I/O operation completed
        B_ERROR = 0x8,   // I/O operation encountered an error
        B_BUSY  = 0x10,  // Buffer is busy
        B_WANTED = 0x20, // Buffer is wanted by another process
        B_ASYNC  = 0x40, // Asynchronous I/O operation
        B_DELWRI = 0x80  // Delayed write
    };

public:
    unsigned int b_flags; // Buffer flags

    Buf* b_forw = NULL; // Forward pointer in the buffer list
    Buf* b_back = NULL; // Backward pointer in the buffer list

    int b_wcount = 0; // Word count for I/O transfer
    unsigned char* b_addr = NULL; // Memory address of the buffer
    int b_blkno = -1; // Block number on disk
    int b_no = 0; // Buffer number

    // Constructor
    Buf();

    // Reset buffer state
    void Reset();
};
