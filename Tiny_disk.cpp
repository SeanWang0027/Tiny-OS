#include "Tiny_disk.h"

Disk::Disk() 
{    
    diskPointer = fopen("myDisk.img", "rb+");

}

Disk::~Disk() 
{
    if (diskPointer != NULL) {
        fclose(diskPointer);
    }
}
void Disk::Reset() 
{
    if (diskPointer != NULL)
        fclose(diskPointer);
    diskPointer = fopen(diskFileName, "rb+");
}

void Disk::write(const unsigned char* in_buffer, unsigned int in_size, int offset, unsigned int origin) {
    if (offset >= 0)
        fseek(diskPointer, offset, origin);
    fwrite(in_buffer, in_size, 1, diskPointer);
    return;
}

void Disk::read(unsigned char* out_buffer, unsigned int out_size, int offset, unsigned int origin) {
    if (offset >= 0)
        fseek(diskPointer, offset, origin);
    fread(out_buffer, out_size, 1, diskPointer);
    return;
}

bool Disk::Exists() 
{
    return diskPointer != NULL;
}

void Disk::Construct() 
{
    diskPointer = fopen(diskFileName, "wb+");
    if (diskPointer == NULL)
        cout << "Fail to open the img file!" << endl;
}