#include "Tiny_system.h"
#include "Tiny_user.h"
#include "Tiny_DirectoryEntry.h"
extern BufferManager myBufferManager;
extern OpenFileTable myOpenFileTable;
extern FileSystem myFileSystem;
extern InodeTable myInodeTable;
extern User myUser;

SystemCall::SystemCall() 
{
	// Get the Inode for the root directory based on its on-disk Inode number.
	// If the Inode is already in memory, return it;
	// if not, read it into memory, lock it, and return it.
	rootDirInode = myInodeTable.IGet(FileSystem::ROOT_INODE_NO);
	rootDirInode->i_count += 0xff; // Increase the reference count
}

void SystemCall::Open()
{
	Inode* pInode;

	pInode = this->NameI(SystemCall::OPEN); // 0 = Open, not create
	// If the corresponding Inode is not found
	if (pInode == NULL) {
		return;
	}
	this->Open_(pInode, 0);
}

/*
 * Function: Create a new file
 * Effect: Establish the open file structure, unlock the memory inode, and set i_count to a positive number (usually 1)
 */
void SystemCall::Creat()
{
	Inode* pInode;
	unsigned int newACCMode = myUser.u_arg[1];
	// Search directory mode is 1, indicating creation; if the parent directory is not writable, return with an error
	pInode = this->NameI(SystemCall::CREATE);
	// If the corresponding Inode is not found or an error occurs in NameI
	if (pInode == NULL) {
		if (myUser.u_error)
			return;
		// Create the Inode
		pInode = this->MakNode(newACCMode);
		// If creation fails
		if (pInode == NULL) {
			return;
		}
		/* 
		 * If the desired name does not exist, call open1() with trf = 2.
		 * No need to check permissions, as the newly created file's permissions
		 * are the same as those indicated by the mode parameter.
		 */
		this->Open_(pInode, 2);
	}
	else {
		this->Open_(pInode, 1);
		pInode->i_mode |= newACCMode;
	}
}


/* 
* trf == 0 called by open
* trf == 1 called by creat when a file with the same name is found
* trf == 2 called by creat when a file with the same name is not found, this is the more general case during file creation
* mode parameter: file open mode, indicating whether the operation is read, write, or read/write
*/
void SystemCall::Open_(Inode* pInode, int trf)
{
	/* If a file with the same name is found during file creation, release all the blocks occupied by the file */
	if (trf == 1) {
		pInode->ITrunc();
	}

	/* Unlock the inode! 
	 * Linear directory search involves a lot of disk read/write operations, causing the process to sleep.
	 * Therefore, the process must lock the inodes involved in the operation. This is done by the IGet lock operation in NameI.
	 * At this point, no more operations will cause a process switch, so the inode can be unlocked.
	 */

	/* Allocate an open file control block (File structure) */

	File* pFile = myOpenFileTable.FAlloc();

	if (pFile == NULL) {
		myInodeTable.IPut(pInode);
		return;
	}

	/* Set the file open mode and establish a link between the File structure and the in-memory Inode */
	pFile->f_flag = (File::FREAD | File::FWRITE);
	pFile->f_inode = pInode;

	/* No special device open function */

	/* All resources required to open or create the file have been successfully allocated, return from the function */
	if (myUser.u_error == 0) {
		return;
	} else {
		/* Release the open file descriptor */
		int fd = myUser.u_ar0[User::EAX];
		if (fd != -1) {
			myUser.u_ofiles.SetF(fd, NULL);
			/* Decrease the reference count of the File structure and the Inode. 
			 * If the File structure is not locked and the reference count is zero, the File structure is released.
			 */
			pFile->f_count--;
		}
		myInodeTable.IPut(pInode);
	}
}

void SystemCall::Close()
{
	int fd = myUser.u_arg[0];

	/* Get the open file control block (File structure) */
	File* pFile = myUser.u_ofiles.GetF(fd);
	if (pFile == NULL) {
		return;
	}

	/* Release the open file descriptor fd and decrease the File structure reference count */
	myUser.u_ofiles.SetF(fd, NULL);
	myOpenFileTable.CloseF(pFile);
}

void SystemCall::Seek()
{
	File* pFile;
	int fd = myUser.u_arg[0];

	pFile = myUser.u_ofiles.GetF(fd);
	if (pFile == NULL) {
		return;  /* If FILE does not exist, GetF sets the error code */
	}

	int offset = myUser.u_arg[1];

	/* If myUser.u_arg[2] is between 3 and 5, then the unit changes from bytes to 512 bytes */
	if (myUser.u_arg[2] > 2) {
		offset = offset << 9;
		myUser.u_arg[2] -= 3;
	}

	switch (myUser.u_arg[2]) {
		/* Set read/write position to offset */
		case 0:
			pFile->f_offset = offset;
			break;
		/* Add offset (can be positive or negative) to read/write position */
		case 1:
			pFile->f_offset += offset;
			break;
		/* Adjust read/write position to file length plus offset */
		case 2:
			pFile->f_offset = pFile->f_inode->i_size + offset;
			break;
	}
	cout << "Successfully moved file pointer to " << pFile->f_offset << endl;
}

void SystemCall::Read()
{
	/* Directly call Rdwr() function */
	this->Rdwr(File::FREAD);
}

void SystemCall::Write()
{
	/* Directly call Rdwr() function */
	this->Rdwr(File::FWRITE);
}

void SystemCall::Rdwr(enum File::FileFlags mode)
{
	File* pFile;

	/* Get the open file control block structure based on the system call parameter fd */
	pFile = myUser.u_ofiles.GetF(myUser.u_arg[0]);	/* fd */
	if (pFile == NULL) {
		/* If the open file does not exist, GetF has already set the error code, so no need to set it here */
		/*	myUser.u_error = User::EBADF;	*/
		return;
	}

	myUser.u_IOParam.m_Base = (unsigned char *)myUser.u_arg[1];	
	myUser.u_IOParam.m_Count = myUser.u_arg[2];		/* Number of bytes to read/write */

	/* Ordinary file read/write or special file read/write.
	 * For files, mutual exclusion access is implemented with a granularity of each system call.
	 * For this, the Inode class needs to add two methods: NFlock() and NFrele().
	 * This is not a V6 design. The read, write system calls lock the in-memory inode to provide the process with a consistent file view during IO operations.
	 */
	
	/* Set the starting read position of the file */
	myUser.u_IOParam.m_Offset = pFile->f_offset;
	if (File::FREAD == mode) {
		pFile->f_inode->ReadI();
	} else {
		pFile->f_inode->WriteI();
	}

	/* Move the file read/write offset pointer based on the number of bytes read/written */
	pFile->f_offset += (myUser.u_arg[2] - myUser.u_IOParam.m_Count);

	/* Return the actual number of bytes read/written, modifying the core stack unit that stores the system call return value */
	myUser.u_ar0[User::EAX] = myUser.u_arg[2] - myUser.u_IOParam.m_Count;
}

Inode* SystemCall::NameI(enum DirectorySearchMode mode)
{
	Inode* pInode;
	Buf* pBuf;
	unsigned int index = 0, nindex = 0;
	int freeEntryOffset;
	pInode = myUser.u_cdir;
	if (myUser.dirp[0] == '/') {
		pInode = this->rootDirInode;
		nindex = ++index + 1;
	}
	while (1) {
		if (myUser.u_error != User::U_NOERROR || nindex >= myUser.dirp.length()) {
			break;
		}
		if ((pInode->i_mode & Inode::IFMT) != Inode::IFDIR) {
			myUser.u_error = User::U_ENOTDIR;
			break;
		}
		nindex = myUser.dirp.find_first_of('/', index);
		memset(myUser.u_dbuf, 0, sizeof(myUser.u_dbuf));
		memcpy(myUser.u_dbuf, myUser.dirp.data() + index, (nindex == (unsigned int)string::npos ? myUser.dirp.length() : nindex) - index);
		index = nindex + 1;
		myUser.u_IOParam.m_Offset = 0;
		myUser.u_IOParam.m_Count = pInode->i_size / (DirectoryEntry::DIRSIZ + 4);
		freeEntryOffset = 0;
		pBuf = NULL;
		while (1) {
			if (myUser.u_IOParam.m_Count == 0) {
				if (pBuf != NULL) {
					myBufferManager.Brelse(pBuf);
				}
				if (nindex >= myUser.dirp.length() && SystemCall::CREATE == mode) {
					myUser.u_pdir = pInode;
					if (!freeEntryOffset) {
						pInode->i_flag |= Inode::IUPD;
					} else {
						myUser.u_IOParam.m_Offset = freeEntryOffset - (DirectoryEntry::DIRSIZ + 4);
					}
					return NULL;
				}
				myInodeTable.IPut(pInode);
				myUser.u_error = User::U_ENOENT;
				return NULL;
			}
			if (myUser.u_IOParam.m_Offset % Inode::BLOCK_SIZE == 0) {
				if (pBuf != NULL) {
					myBufferManager.Brelse(pBuf);
				}
				int phyBlkno = pInode->Bmap(myUser.u_IOParam.m_Offset / Inode::BLOCK_SIZE);
				pBuf = myBufferManager.Bread(phyBlkno);
			}
			memcpy(&myUser.u_dent, pBuf->b_addr + (myUser.u_IOParam.m_Offset % Inode::BLOCK_SIZE), sizeof(myUser.u_dent));
			myUser.u_IOParam.m_Offset += sizeof(DirectoryEntry);
			myUser.u_IOParam.m_Count -= 1;
			if (myUser.u_dent.m_ino == 0) {
				if (freeEntryOffset == 0)
					freeEntryOffset = myUser.u_IOParam.m_Offset;
				continue;
			}
			if (!memcmp(myUser.u_dbuf, &myUser.u_dent.m_name, sizeof(DirectoryEntry) - 4))
				break;
		}
		if (SystemCall::DELETE == mode && nindex >= myUser.dirp.length()) {
			return pInode;
		}
		if (pBuf != NULL) {
			myBufferManager.Brelse(pBuf);
		}
		myInodeTable.IPut(pInode);
		pInode = myInodeTable.IGet(myUser.u_dent.m_ino);
		if (pInode == NULL) {
			return NULL;
		}
	}
	return NULL;
}

/* Called by creat().
 * Write a new inode and new directory entry for the newly created file.
 * The returned pInode is a locked in-memory inode with i_count set to 1.
 *
 * At the end of the function, WriteDir is called to write the directory entry into the parent directory, update the parent directory inode, and write it back to the disk.
 */
Inode* SystemCall::MakNode(int mode)
{
	Inode* pInode;

	/* Allocate a free DiskInode with all contents cleared */
	pInode = myFileSystem.IAlloc();
	if (pInode == NULL) {
		return NULL;
	}

	pInode->i_flag |= (Inode::IACC | Inode::IUPD);
	pInode->i_mode = mode | Inode::IALLOC;
	pInode->i_nlink = 1;

	/* Write the directory entry into myUser.u_dent, and then write it into the directory file */
	this->WriteDir(pInode);
	return pInode;
}

void SystemCall::WriteDir(Inode* pInode)
{
	/* Set the inode number in the directory entry */
	myUser.u_dent.m_ino = pInode->i_number;

	/* Set the pathname component in the directory entry */
	for (int i = 0; i < DirectoryEntry::DIRSIZ; i++) {
		myUser.u_dent.m_name[i] = myUser.u_dbuf[i];
	}

	myUser.u_IOParam.m_Count = DirectoryEntry::DIRSIZ + 4;
	myUser.u_IOParam.m_Base = (unsigned char *)&myUser.u_dent;

	/* Write the directory entry into the parent directory file */
	myUser.u_pdir->WriteI();
	myInodeTable.IPut(myUser.u_pdir);
}

void SystemCall::UnLink()
{
	Inode* pInode;
	Inode* pDeleteInode;

	pDeleteInode = this->NameI(SystemCall::DELETE);
	if (pDeleteInode == NULL) {
		return;
	}

	pInode = myInodeTable.IGet(myUser.u_dent.m_ino);
	if (pInode == NULL) {
		return;
	}

	myUser.u_IOParam.m_Offset -= (DirectoryEntry::DIRSIZ + 4);
	myUser.u_IOParam.m_Base = (unsigned char *)&myUser.u_dent;
	myUser.u_IOParam.m_Count = DirectoryEntry::DIRSIZ + 4;
	
	myUser.u_dent.m_ino = 0;
	pDeleteInode->WriteI();

	pInode->i_nlink--;
	pInode->i_flag |= Inode::IUPD;

	myInodeTable.IPut(pDeleteInode);
	myInodeTable.IPut(pInode);
}


void SystemCall::ChDir()
{
	Inode* pInode;

	pInode = this->NameI(SystemCall::OPEN);
	if ( NULL == pInode )
	{
		return;
	}
	if ( (pInode->i_mode & Inode::IFMT) != Inode::IFDIR )
	{
		myUser.u_error = User::U_ENOTDIR;
		myInodeTable.IPut(pInode);
		return;
	}
	myUser.u_cdir = pInode;
	if (myUser.dirp[0] != '/') {
	    myUser.curDirPath += myUser.dirp;
	}
	else {
		myUser.curDirPath = myUser.dirp;
	}
	if (myUser.curDirPath.back() != '/'){
		myUser.curDirPath.push_back('/');
	}

}

void SystemCall::Ls()  
{
	Inode* pInode = myUser.u_cdir;
	Buf* pBuf = NULL;
	myUser.u_IOParam.m_Offset = 0;
	myUser.u_IOParam.m_Count = pInode->i_size / sizeof(DirectoryEntry);
	while (myUser.u_IOParam.m_Count) {
		if (0 == myUser.u_IOParam.m_Offset % Inode::BLOCK_SIZE) {
			if (pBuf)
				myBufferManager.Brelse(pBuf);
			int phyBlkno = pInode->Bmap(myUser.u_IOParam.m_Offset / Inode::BLOCK_SIZE);
			pBuf = myBufferManager.Bread(phyBlkno);
		}
		memcpy(&myUser.u_dent, pBuf->b_addr + (myUser.u_IOParam.m_Offset % Inode::BLOCK_SIZE), sizeof(myUser.u_dent));
		myUser.u_IOParam.m_Offset += sizeof(DirectoryEntry);
		myUser.u_IOParam.m_Count--;

		if (0 == myUser.u_dent.m_ino)
			continue;

		myUser.ls_str += myUser.u_dent.m_name;
		myUser.ls_str += "\n";
	}	
	if (pBuf)
		myBufferManager.Brelse(pBuf);
}