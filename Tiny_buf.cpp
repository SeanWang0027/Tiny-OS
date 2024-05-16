#include "Tiny_buf.h"
extern Disk myDisk;
Buf::Buf() 
{
    return;
}

void Buf::Reset() 
{
    this->b_flags = 0;
    this->b_forw = NULL;
    this->b_back = NULL;
    this->b_wcount = 0;
    this->b_addr = NULL;
    this->b_blkno = -1;
    this->b_no = 0;
}
