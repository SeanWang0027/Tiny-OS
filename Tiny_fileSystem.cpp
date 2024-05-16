#include "Tiny_fileSystem.h"
#include "Tiny_user.h"
#include "Tiny_disk.h"
#include "Tiny_openFileManager.h"
#include "Tiny_cache.h"
#include "Tiny_buf.h"
#include "Tiny_Superblock.h"
#include "Tiny_DiskNode.h"
extern User myUser;
extern Disk myDisk;
extern BufferManager myBufferManager;
extern SuperBlock mySuperBlock;
extern InodeTable myInodeTable;

/*==============================class FileSystem===================================*/
FileSystem::FileSystem()
{
    if (!myDisk.Exists())
		FormatDevice();
	else
		LoadSuperBlock();
}

FileSystem::~FileSystem()
{
    Update();
}

void FileSystem::FormatDevice() 
{
	/*
	Format SuperBlock
	*/
	mySuperBlock.s_isize = FileSystem::INODE_SECTOR_NUMBER;
	mySuperBlock.s_fsize = FileSystem::DISK_SECTOR_NUMBER;
	mySuperBlock.s_nfree = 0;
	mySuperBlock.s_free[0] = -1;
	mySuperBlock.s_ninode = 0;
	mySuperBlock.s_fmod = 0;
	time((time_t*)&mySuperBlock.s_time);
	
	myDisk.Construct();
	myDisk.write((unsigned char*)(&mySuperBlock), sizeof(SuperBlock), 0);

	DiskInode emptyDINode, rootDINode;
	rootDINode.d_mode |= Inode::IALLOC | Inode::IFDIR;
	myDisk.write((unsigned char*)&rootDINode, sizeof(rootDINode));
	for (int i = 1; i < FileSystem::INODE_NUMBER_ALL; ++i) {
		if (mySuperBlock.s_ninode < 100)
			mySuperBlock.s_inode[mySuperBlock.s_ninode++] = i;
		myDisk.write((unsigned char*)&emptyDINode, sizeof(emptyDINode));
	}
	char freeBlock[BLOCK_SIZE], freeBlock1[BLOCK_SIZE];
	memset(freeBlock, 0, BLOCK_SIZE);
	memset(freeBlock1, 0, BLOCK_SIZE);

	for (int i = 0; i < FileSystem::DATA_SECTOR_NUMBER; ++i) {
		if (mySuperBlock.s_nfree >= 100) {
			memcpy(freeBlock1, &mySuperBlock.s_nfree, sizeof(int) + sizeof(mySuperBlock.s_free));
			myDisk.write((unsigned char*)&freeBlock1, BLOCK_SIZE);
			mySuperBlock.s_nfree = 0;
		}
		else
			myDisk.write((unsigned char*)freeBlock, BLOCK_SIZE);
		mySuperBlock.s_free[mySuperBlock.s_nfree++] = i + FileSystem::DATA_START_SECTOR;
	}

	time((time_t*)&mySuperBlock.s_time);
	myDisk.write((unsigned char*)(&mySuperBlock), sizeof(SuperBlock), 0);
}


void FileSystem::LoadSuperBlock()
{
	fseek(myDisk.diskPointer, 0, SEEK_SET);
	myDisk.read((unsigned char *)(&mySuperBlock), sizeof(SuperBlock), 0);
}

void FileSystem::Update()
{
	Buf* pBuf;
	mySuperBlock.s_fmod = 0;
	mySuperBlock.s_time = (int)time(NULL);
	for (int j = 0; j < 2; j++) {
		int* p = (int*)&mySuperBlock + j * 128;
		pBuf = myBufferManager.GetBlk(FileSystem::SUPERBLOCK_START_SECTOR + j);
		memcpy(pBuf->b_addr, p, BLOCK_SIZE);
		myBufferManager.Bwrite(pBuf);
	}
	myInodeTable.UpdateInodeTable();
	myBufferManager.Bflush();
}

Inode* FileSystem::IAlloc()
{
	Buf* pBuf;
	Inode* pNode;
	int ino;	/* 分配到的空闲外存Inode编号 */
	if(mySuperBlock.s_ninode <= 0) {
		/* 外存Inode编号从0开始，这不同于Unix V6中外存Inode从1开始编号 */
		ino = -1;

		/* 依次读入磁盘Inode区中的磁盘块，搜索其中空闲外存Inode，记入空闲Inode索引表 */
		for(int i = 0; i < mySuperBlock.s_isize; i++) {
			pBuf = myBufferManager.Bread(FileSystem::INODE_ZONE_START_SECTOR + i);

			/* 获取缓冲区首址 */
			int* p = (int *)pBuf->b_addr;

			/* 检查该缓冲区中每个外存Inode的i_mode != 0，表示已经被占用 */
			for(int j = 0; j < FileSystem::INODE_NUMBER_PER_SECTOR; j++){
				ino++;
				int mode = *( p + j * sizeof(DiskInode)/sizeof(int) );
				/* 该外存Inode已被占用，不能记入空闲Inode索引表 */
				if(mode) 
					continue;
				/* 
				 * 如果外存inode的i_mode==0，此时并不能确定
				 * 该inode是空闲的，因为有可能是内存inode没有写到
				 * 磁盘上,所以要继续搜索内存inode中是否有相应的项
				 */
				if(myInodeTable.IsLoaded(ino) == -1 ) {
					/* 该外存Inode没有对应的内存拷贝，将其记入空闲Inode索引表 */
					mySuperBlock.s_inode[mySuperBlock.s_ninode++] = ino;

					/* 如果空闲索引表已经装满，则不继续搜索 */
					if(mySuperBlock.s_ninode >= 100)	{
						break;
					}
				}
			}

			/* 至此已读完当前磁盘块，释放相应的缓存 */
			myBufferManager.Brelse(pBuf);

			/* 如果空闲索引表已经装满，则不继续搜索 */
			if(mySuperBlock.s_ninode >= 100) {
				break;
			}
		}
		
		/* 如果在磁盘上没有搜索到任何可用外存Inode，返回NULL */
		if(mySuperBlock.s_ninode <= 0){
			myUser.u_error = User::U_ENOSPC;
			return NULL;
		}
	}

	/* 
	 * 上面部分已经保证，除非系统中没有可用外存Inode，
	 * 否则空闲Inode索引表中必定会记录可用外存Inode的编号。
	 */
	
	/* 从索引表“栈顶”获取空闲外存Inode编号 */
	ino = mySuperBlock.s_inode[--mySuperBlock.s_ninode];

	/* 将空闲Inode读入内存 */
	pNode = myInodeTable.IGet(ino);
	/* 未能分配到内存inode */
	if(NULL == pNode) {
		cout << "无空闲内存可分配给INODE！" << endl;
		return NULL;
	}

	/* 如果该Inode空闲,清空Inode中的数据 */
	
	pNode->Clean();
	/* 设置SuperBlock被修改标志 */
	mySuperBlock.s_fmod = 1;
	return pNode;
}

void FileSystem::IFree(int number)
{	
	/* 
	 * 如果超级块直接管理的空闲外存Inode超过100个，
	 * 同样让释放的外存Inode散落在磁盘Inode区中。
	 */
	if(mySuperBlock.s_ninode >= 100)	{
		return;
	}
	mySuperBlock.s_inode[mySuperBlock.s_ninode++] = number;

	/* 设置SuperBlock被修改标志 */
	mySuperBlock.s_fmod = 1;
}

Buf* FileSystem::Alloc()
{
	int blkno;	/* 分配到的空闲磁盘块编号 */
	Buf* pBuf;
	/* 从索引表“栈顶”获取空闲磁盘块编号 */
	blkno = mySuperBlock.s_free[--mySuperBlock.s_nfree];

	/* 
	 * 若获取磁盘块编号为零，则表示已分配尽所有的空闲磁盘块。
	 * 或者分配到的空闲磁盘块编号不属于数据盘块区域中(由BadBlock()检查)，
	 * 都意味着分配空闲磁盘块操作失败。
	 */
	if(blkno <= 0){
		mySuperBlock.s_nfree = 0;
		myUser.u_error = User::U_ENOSPC;
		return NULL;
	}

	/* 
	 * 栈已空，新分配到空闲磁盘块中记录了下一组空闲磁盘块的编号,
	 * 将下一组空闲磁盘块的编号读入SuperBlock的空闲磁盘块索引表s_free[100]中。
	 */
	if(mySuperBlock.s_nfree <= 0){
		/* 
		 * 此处加锁，因为以下要进行读盘操作，有可能发生进程切换，
		 * 新上台的进程可能对SuperBlock的空闲盘块索引表访问，会导致不一致性。
		 */
		/* 读入该空闲磁盘块 */
		pBuf = myBufferManager.Bread(blkno);

		/* 从该磁盘块的0字节开始记录，共占据4(s_nfree)+400(s_free[100])个字节 */
		int* p = (int *)pBuf->b_addr;

		/* 首先读出空闲盘块数s_nfree */
		mySuperBlock.s_nfree = *p++;
		memcpy(mySuperBlock.s_free, p, sizeof(mySuperBlock.s_free));
		/* 缓存使用完毕，释放以便被其它进程使用 */
		myBufferManager.Brelse(pBuf);

		/* 解除对空闲磁盘块索引表的锁，唤醒因为等待锁而睡眠的进程 */
	}

	/* 普通情况下成功分配到一空闲磁盘块 */
	pBuf = myBufferManager.GetBlk(blkno);	/* 为该磁盘块申请缓存 */
	if (pBuf)
		myBufferManager.Bclear(pBuf);	/* 清空缓存中的数据 */
	mySuperBlock.s_fmod = 1;	/* 设置SuperBlock被修改标志 */

	return pBuf;
}

void FileSystem::Free(int blkno)
{
	Buf* pBuf;
	/* 
	 * 如果先前系统中已经没有空闲盘块，
	 * 现在释放的是系统中第1块空闲盘块
	 */
	if(mySuperBlock.s_nfree <= 0) {
		mySuperBlock.s_nfree = 1;
		mySuperBlock.s_free[0] = 0;	/* 使用0标记空闲盘块链结束标志 */
	}

	/* SuperBlock中直接管理空闲磁盘块号的栈已满 */
	if(mySuperBlock.s_nfree >= 100)	{
		/* 
		 * 使用当前Free()函数正要释放的磁盘块，存放前一组100个空闲
		 * 磁盘块的索引表
		 */
		pBuf = myBufferManager.GetBlk(blkno);	/* 为当前正要释放的磁盘块分配缓存 */

		/* 从该磁盘块的0字节开始记录，共占据4(s_nfree)+400(s_free[100])个字节 */
		int* p = (int *)pBuf->b_addr;
		
		/* 首先写入空闲盘块数，除了第一组为99块，后续每组都是100块 */
		*p++ = mySuperBlock.s_nfree;
		memcpy(p, mySuperBlock.s_free, sizeof(int) * 100);
		mySuperBlock.s_nfree = 0;
		/* 将存放空闲盘块索引表的“当前释放盘块”写入磁盘，即实现了空闲盘块记录空闲盘块号的目标 */
		myBufferManager.Bwrite(pBuf);
	}
	mySuperBlock.s_free[mySuperBlock.s_nfree++] = blkno;	/* SuperBlock中记录下当前释放盘块号 */
	mySuperBlock.s_fmod = 1;
}
