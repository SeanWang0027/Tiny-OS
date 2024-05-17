#include "Tiny_buf.h"
extern Disk myDisk;
Buf::Buf() 
{
    return;
}

void Buf::Reset() 
{
    this->b_flags = this->b_wcount = this->b_no =  0;
    this->b_forw = NULL; 
    this->b_back = NULL;
    this->b_addr =  NULL;
    this->b_blkno = -1;
}
