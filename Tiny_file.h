#pragma once
#include "Tiny_inode.h"
#include <cstring>
#include <ctime>

class File
{
public:
    // Enumeration for file access flags
    enum FileFlags {
        FREAD = 0x1,    // File read flag
        FWRITE = 0x2,   // File write flag
    };
    unsigned int f_flag;   // File access mode flags
    int f_count;           // Reference count
    Inode* f_inode;        // Pointer to the inode
    int f_offset;          // File pointer offset

    // Constructor
    File();
    
    // Destructor
    ~File();
    
    // Reset file state
    void Reset();
};


class OpenFiles 
{
public:
    static const int NOFILES = 50; // Maximum number of open files

    // Constructor
    OpenFiles();
    
    // Destructor
    ~OpenFiles();
    
    // Reset open files table
    void Reset();
    
    // Allocate a free slot in the open files table
    int AllocFreeSlot();
    
    // Get the file object at the given file descriptor
    File* GetF(int fd);
    
    // Set the file object at the given file descriptor
    void SetF(int fd, File* pFile);

    bool Judge(int fd);

private:
    File* processOpenFileTable[NOFILES]; // Array of pointers to open files
};


class IOParameter 
{
public:
    // Constructor
    IOParameter();
    
    unsigned char* m_Base = 0;  // Base address of the buffer
    int m_Offset = 0;           // File offset for the operation
    int m_Count = 0;            // Number of bytes to read/write
};