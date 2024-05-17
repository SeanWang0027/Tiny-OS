#include "Tiny_openFileManager.h"
#include "Tiny_user.h"
#include <ctime>
extern User myUser;
extern BufferManager myBufferManager;
extern FileSystem myFileSystem;
extern InodeTable myInodeTable;

void OpenFileTable::Reset() 
{
	for (int i = 0; i < OpenFileTable::NFILE; i++)
		m_File[i].Reset();
}

File* OpenFileTable::FAlloc()
{
	int fd;

	/* Get a free slot in the process open file descriptor table */
	fd = myUser.u_ofiles.AllocFreeSlot();

	if (fd < 0) {
		return NULL;
	}

	for (int i = 0; i < OpenFileTable::NFILE; i++) {
		/* f_count == 0 indicates that this entry is free */
		if (this->m_File[i].f_count == 0) {
			/* Establish the link between the descriptor and the File structure */
			myUser.u_ofiles.SetF(fd, &this->m_File[i]);
			/* Increase the reference count of the file structure */
			this->m_File[i].f_count++;
			/* Clear the file read/write position */
			this->m_File[i].f_offset = 0;
			return (&this->m_File[i]);
		}
	}
	myUser.u_error = User::U_ENFILE;
	return NULL;
}


void OpenFileTable::CloseF(File *pFile)
{
	if(pFile->f_count <= 1)	{
		myInodeTable.IPut(pFile->f_inode);
	}

	pFile->f_count--;
}

InodeTable::InodeTable()
{

}

void InodeTable::Reset()
{
    Inode emptyInode;
	for (int i = 0; i < InodeTable::NINODE; i++)
		m_Inode[i].Reset();
}

Inode* InodeTable::IGet(int inumber)
{
	Inode* pInode;
    int index = this->IsLoaded(inumber);
    if(index >= 0)	{
        pInode = &(this->m_Inode[index]);
        pInode->i_count++;
        return pInode;
    }
    else {
        pInode = this->GetFreeInode();
        if(NULL == pInode)	{
            cout << "Inode Table is full!" << endl;
            myUser.u_error = User::U_ENFILE;
            return NULL;
        }
        else{
            pInode->i_number = inumber;
            pInode->i_count++;
            Buf* pBuf = myBufferManager.Bread(FileSystem::INODE_ZONE_START_SECTOR + inumber / FileSystem::INODE_NUMBER_PER_SECTOR );
			pInode->ICopy(pBuf, inumber);
            myBufferManager.Brelse(pBuf);
            return pInode;
        }
    }
	
	return NULL;	/* GCC likes it! */
}

/* When closing a file, Iput is called
 *      Main actions: Decrease memory inode count (i_count--); if zero, free memory inode and write back to disk if modified
 * All directory files traversed during file search will have their memory inodes Iput after traversal.
 *   The second last path component in a pathname is always a directory file. If creating or deleting a file within it,
 *   or creating or deleting a subdirectory, the memory inode of this directory file must be written back to the disk.
 *   Regardless of whether this directory file has been modified, its inode must be written back to the disk.
 */
void InodeTable::IPut(Inode *pNode)
{
	/* The current process is the only one referencing this memory inode and is ready to release it */
	if (pNode->i_count == 1) {

		/* The file is no longer pointed to by any directory path */
		if (pNode->i_nlink <= 0) {
			/* Free the data blocks occupied by this file */
			pNode->ITrunc();
			pNode->i_mode = 0;
			/* Free the corresponding on-disk inode */
			myFileSystem.IFree(pNode->i_number);
		}

		/* Update the on-disk inode information */
		pNode->IUpdate((int)time(NULL));
		/* Clear all flags of the memory inode */
		pNode->i_flag = 0;
		/* This is one of the indicators that the memory inode is free, the other being i_count == 0 */
		pNode->i_number = -1;
	}

	/* Decrease the reference count of the memory inode and wake up waiting processes */
	pNode->i_count--;
}


void InodeTable::UpdateInodeTable()
{
	for (int i = 0; i < InodeTable::NINODE; i++) {
		/* 
		 * If the Inode object is not locked, i.e., it is not currently being used by other processes, it can be synchronized to the on-disk Inode;
		 * and count is not zero. count == 0 means this memory Inode is not referenced by any open files and does not need to be synchronized.
		 */
		if (this->m_Inode[i].i_count != 0) {
			this->m_Inode[i].IUpdate((int)time(NULL));
		}
	}
}

int InodeTable::IsLoaded(int inumber)
{
	for (int i = 0; i < InodeTable::NINODE; i++) {
		if (this->m_Inode[i].i_number == inumber && this->m_Inode[i].i_count != 0) {
			return i;	
		}
	}
	return -1;
}

Inode* InodeTable::GetFreeInode()
{
	for (int i = 0; i < InodeTable::NINODE; i++) {
		if (this->m_Inode[i].i_count == 0) {
			return &(this->m_Inode[i]);
		}
	}
	return NULL;
}

