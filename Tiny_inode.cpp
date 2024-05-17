#include "Tiny_inode.h"
#include "Tiny_fileSystem.h"
#include "Tiny_user.h"
#include "Tiny_DiskNode.h"

extern User myUser;
extern BufferManager myBufferManager;
extern FileSystem myFileSystem;
Inode::Inode()
{	
	for(int i = 0; i < 10; i++) {
		this->i_addr[i] = 0;
	}
}

void Inode::Reset()
{
	i_mode = 0;
	i_count = 0;
	i_number = -1;
	i_size = 0;
	memset(i_addr, 0, sizeof(i_addr));
}

Inode::~Inode()
{

}

void Inode::ReadI()
{
	int lbn;	/* Logical block number */
	int bn;		/* Physical block number corresponding to lbn */
	int offset;	/* Starting position within the current block */
	int nbytes;	/* Number of bytes to transfer to user space */
	Buf* pBuf;

	if (0 == myUser.u_IOParam.m_Count)	{
		/* If the number of bytes to read is zero, return */
		return;
	}

	this->i_flag |= Inode::IACC;

	/* Read all required data one block at a time until the end of the file is reached */
	while (User::U_NOERROR == myUser.u_error && myUser.u_IOParam.m_Count != 0) {
		lbn = bn = myUser.u_IOParam.m_Offset / Inode::BLOCK_SIZE;
		offset = myUser.u_IOParam.m_Offset % Inode::BLOCK_SIZE;
		/* Number of bytes to transfer to user space, limited by the smaller of the remaining bytes requested and the available bytes in the current block */
		nbytes = Inode::BLOCK_SIZE - offset < myUser.u_IOParam.m_Count ? Inode::BLOCK_SIZE - offset : myUser.u_IOParam.m_Count;

		int remain = this->i_size - myUser.u_IOParam.m_Offset;
		/* If we have read past the end of the file */
		if (remain <= 0) {
			return;
		}
		/* The number of bytes to transfer also depends on the remaining file size */
		nbytes = min(nbytes, remain);

		/* Convert logical block number lbn to physical block number bn. Bmap sets Inode::rablock.
		 * UNIX may skip read-ahead if the cost is too high, setting Inode::rablock to 0.
		 */
		if ((bn = this->Bmap(lbn)) == 0) {
			return;
		}

		/* Note: this differs from previous implementations */
		pBuf = myBufferManager.Bread(bn);

		/* Record the logical block number of the last read block */
		this->i_lastr = lbn;

		/* Starting read position in the buffer */
		unsigned char* start = pBuf->b_addr + offset;
		memcpy(myUser.u_IOParam.m_Base, start, nbytes);
		/* Update read/write position with the number of bytes transferred */
		myUser.u_IOParam.m_Base += nbytes;
		myUser.u_IOParam.m_Offset += nbytes;
		myUser.u_IOParam.m_Count -= nbytes;

		myBufferManager.Brelse(pBuf);	/* Release the buffer resource after use */
	}
}

void Inode::WriteI()
{
	int lbn;	/* Logical block number */
	int bn;		/* Physical block number corresponding to lbn */
	int offset;	/* Starting position within the current block */
	int nbytes;	/* Number of bytes to transfer */
	Buf* pBuf;

	/* Set inode access flags */
	this->i_flag |= (Inode::IACC | Inode::IUPD);

	if (0 == myUser.u_IOParam.m_Count) {
		/* If the number of bytes to write is zero, return */
		return;
	}

	while (User::U_NOERROR == myUser.u_error && myUser.u_IOParam.m_Count != 0) {
		lbn = myUser.u_IOParam.m_Offset / Inode::BLOCK_SIZE;
		offset = myUser.u_IOParam.m_Offset % Inode::BLOCK_SIZE;
		nbytes = Inode::BLOCK_SIZE - offset < myUser.u_IOParam.m_Count ? Inode::BLOCK_SIZE - offset : myUser.u_IOParam.m_Count;
		if ((bn = this->Bmap(lbn)) == 0)
			return;
		if (Inode::BLOCK_SIZE == nbytes) {
			/* If writing a full block, allocate buffer */
			pBuf = myBufferManager.GetBlk(bn);
		} else {
			/* If writing less than a block, read then write (protect unwritten data) */
			pBuf = myBufferManager.Bread(bn);
		}

		/* Starting write position in the buffer */
		unsigned char* start = pBuf->b_addr + offset;

		/* Write operation: copy data from user space to buffer */
		memcpy(start, myUser.u_IOParam.m_Base, nbytes);

		/* Update read/write position with number of bytes transferred */
		myUser.u_IOParam.m_Base += nbytes;
		myUser.u_IOParam.m_Offset += nbytes;
		myUser.u_IOParam.m_Count -= nbytes;

		/* If an error occurs during writing */
		if (myUser.u_error != User::U_NOERROR) {
			myBufferManager.Brelse(pBuf);
		}
		
		/* Mark buffer for delayed write, not urgent to output block to disk */
		myBufferManager.Bdwrite(pBuf);
		
		/* Increase file size for regular files */
		if (this->i_size < myUser.u_IOParam.m_Offset) {
			this->i_size = myUser.u_IOParam.m_Offset;
		}

		this->i_flag |= Inode::IUPD;
	}
}


void Inode::ICopy(Buf* pb, int inumber) 
{
	DiskInode& dInode = *(DiskInode*)(pb->b_addr + (inumber % FileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode));
	i_mode = dInode.d_mode;
	i_size = dInode.d_size;
	i_nlink = dInode.d_nlink;
	for(int i = 0; i < 10; i++) {
		this->i_addr[i] = dInode.d_addr[i];
	}
}

int Inode::Bmap(int lbn)
{
	Buf* pFirstBuf;
	Buf* pSecondBuf;
	int phyBlkno;
	int* iTable;
	int index;

	if (lbn >= Inode::HUGE_FILE_BLOCK) {
		myUser.u_error = User::U_EFBIG;
		return 0;
	}

	if (lbn < 6) {
		phyBlkno = this->i_addr[lbn];
		if (phyBlkno == 0 && (pFirstBuf = myFileSystem.Alloc()) != NULL) {
			myBufferManager.Bdwrite(pFirstBuf);
			phyBlkno = pFirstBuf->b_blkno;
			this->i_addr[lbn] = phyBlkno;
			this->i_flag |= Inode::IUPD;
		}
		return phyBlkno;
	} else {
		if (lbn < Inode::LARGE_FILE_BLOCK) {
			index = (lbn - Inode::SMALL_FILE_BLOCK) / Inode::ADDRESS_PER_INDEX_BLOCK + 6;
		} else {
			index = (lbn - Inode::LARGE_FILE_BLOCK) / (Inode::ADDRESS_PER_INDEX_BLOCK * Inode::ADDRESS_PER_INDEX_BLOCK) + 8;
		}
		phyBlkno = this->i_addr[index];
		if (phyBlkno == 0) {
			this->i_flag |= Inode::IUPD;
			if ((pFirstBuf = myFileSystem.Alloc()) == NULL) {
				return 0;
			}
			this->i_addr[index] = pFirstBuf->b_blkno;
		} else {
			pFirstBuf = myBufferManager.Bread(phyBlkno);
		}
		iTable = (int *)pFirstBuf->b_addr;

		if (index >= 8) {
			index = ((lbn - Inode::LARGE_FILE_BLOCK) / Inode::ADDRESS_PER_INDEX_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK;
			phyBlkno = iTable[index];
			if (phyBlkno == 0) {
				if ((pSecondBuf = myFileSystem.Alloc()) == NULL) {
					myBufferManager.Brelse(pFirstBuf);
					return 0;
				}
				iTable[index] = pSecondBuf->b_blkno;
				myBufferManager.Bdwrite(pFirstBuf);
			} else {
				myBufferManager.Brelse(pFirstBuf);
				pSecondBuf = myBufferManager.Bread(phyBlkno);
			}

			pFirstBuf = pSecondBuf;
			iTable = (int *)pSecondBuf->b_addr;
		}

		if (lbn < Inode::LARGE_FILE_BLOCK) {
			index = (lbn - Inode::SMALL_FILE_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK;
		} else {
			index = (lbn - Inode::LARGE_FILE_BLOCK) % Inode::ADDRESS_PER_INDEX_BLOCK;
		}

		if ((phyBlkno = iTable[index]) == 0 && (pSecondBuf = myFileSystem.Alloc()) != NULL) {
			phyBlkno = pSecondBuf->b_blkno;
			iTable[index] = phyBlkno;
			myBufferManager.Bdwrite(pSecondBuf);
			myBufferManager.Bdwrite(pFirstBuf);
		} else {
			myBufferManager.Brelse(pFirstBuf);
		}
		return phyBlkno;
	}
}


void Inode::IUpdate(int time)
{
	Buf* pBuf;
	DiskInode dInode;

	/* Update the corresponding DiskInode only if either IUPD or IACC flag is set.
	 * Directory searches do not set the IACC and IUPD flags on the traversed directory files. */
	if ((this->i_flag & (Inode::IUPD | Inode::IACC)) != 0) {

		pBuf = myBufferManager.Bread(FileSystem::INODE_ZONE_START_SECTOR + this->i_number / FileSystem::INODE_NUMBER_PER_SECTOR);

		/* Copy the information from the in-memory inode to dInode, 
		 * then overwrite the old on-disk inode in the buffer with dInode. */
		dInode.d_mode = this->i_mode;
		dInode.d_nlink = this->i_nlink;
		dInode.d_uid = this->i_uid;
		dInode.d_gid = this->i_gid;
		dInode.d_size = this->i_size;
		for (int i = 0; i < 10; i++) {
			dInode.d_addr[i] = this->i_addr[i];
		}
		if (this->i_flag & Inode::IACC) {
			/* Update last access time */
			dInode.d_atime = time;
		}
		if (this->i_flag & Inode::IUPD) {
			/* Update last modification time */
			dInode.d_mtime = time;
		}

		/* Point to the offset position of the old on-disk inode in the buffer */
		unsigned char* p = pBuf->b_addr + (this->i_number % FileSystem::INODE_NUMBER_PER_SECTOR) * sizeof(DiskInode);
		DiskInode* pNode = &dInode;

		/* Overwrite the old on-disk inode in the buffer with the new data from dInode */
		memcpy(p, pNode, sizeof(DiskInode));

		/* Write the buffer back to the disk to update the old on-disk inode */
		myBufferManager.Bwrite(pBuf);
	}
}


void Inode::ITrunc()
{
	/* Release blocks in a FILO manner to keep the free block numbers in the SuperBlock as contiguous as possible.
	 * 
	 * Unix V6++ file indexing structure (small, large, and huge files):
	 * (1) i_addr[0] - i_addr[5] are direct indexes, covering file lengths of 0 - 6 blocks;
	 * 
	 * (2) i_addr[6] - i_addr[7] store the disk block numbers of single indirect index tables, each containing 128 data block numbers, covering file lengths of 7 - (128 * 2 + 6) blocks;
	 *
	 * (3) i_addr[8] - i_addr[9] store the disk block numbers of double indirect index tables, each recording 128 single indirect index table block numbers, covering file lengths of (128 * 2 + 6) < size <= (128 * 128 * 2 + 128 * 2 + 6).
	 */
	for (int i = 9; i >= 0; i--) {
		/* If i_addr[] has an index at position i */
		if (this->i_addr[i] != 0) {
			/* If it's a single or double indirect index in i_addr[] */
			if (i >= 6 && i <= 9) {
				/* Read the indirect index table into the buffer */
				Buf* pFirstBuf = myBufferManager.Bread(this->i_addr[i]);
				/* Get the buffer start address */
				int* pFirst = (int*)pFirstBuf->b_addr;

				/* Each indirect index table records 512/sizeof(int) = 128 block numbers, traverse all 128 blocks */
				for (int j = 128 - 1; j >= 0; j--) {
					/* If this entry has an index */
					if (pFirst[j] != 0) {
						/* If it's a double indirect index table in i_addr[8] or i_addr[9],
						 * this block records 128 block numbers of single indirect index tables
						 */
						if (i >= 8 && i <= 9) {
							Buf* pSecondBuf = myBufferManager.Bread(pFirst[j]);
							int* pSecond = (int*)pSecondBuf->b_addr;

							for (int k = 128 - 1; k >= 0; k--) {
								if (pSecond[k] != 0) {
									/* Free the specified block */
									myFileSystem.Free(pSecond[k]);
								}
							}
							/* Release the buffer after use to be available for other processes */
							myBufferManager.Brelse(pSecondBuf);
						}
						myFileSystem.Free(pFirst[j]);
					}
				}
				myBufferManager.Brelse(pFirstBuf);
			}
			/* Free the block occupied by the index table itself */
			myFileSystem.Free(this->i_addr[i]);
			/* Set to 0 to indicate that this entry does not contain an index */
			this->i_addr[i] = 0;
		}
	}

	/* After freeing blocks, set file size to zero */
	this->i_size = 0;
	/* Set the IUPD flag to indicate that this in-memory inode needs to be synchronized to the corresponding on-disk inode */
	this->i_flag |= Inode::IUPD;
	/* Clear the large file flag and the original RWXRWXRWX bits */
	this->i_mode &= ~(Inode::ILARG);
	this->i_nlink = 1;
}


void Inode::Clean()
{
	this->i_mode = 0;
	this->i_nlink = 0;
	this->i_uid = -1;
	this->i_gid = -1;
	this->i_size = 0;
	this->i_lastr = -1;
	for(int i = 0; i < 10; i++)
	{
		this->i_addr[i] = 0;
	}
}