#include "Tiny_DiskNode.h"

DiskInode::DiskInode()
{
	for(int i = 0; i < 10; i++)	{
		this->d_addr[i] = 0;
	}
}

DiskInode::~DiskInode()
{
	return;
}