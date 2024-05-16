#pragma once
#include <string>
#include "Tiny_cache.h"
#include "Tiny_buf.h"
#include <ctime>

class DiskInode 
{
public:
    unsigned int d_mode = 0;
    int d_nlink = 0;
    short d_uid = -1;
    short d_gid = -1;
    int d_size = 0;
    int d_addr[10];
    int d_atime = 0;
    int d_mtime = 0;

    DiskInode();
    ~DiskInode();
};