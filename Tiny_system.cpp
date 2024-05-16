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
	rootDirInode = myInodeTable.IGet(FileSystem::ROOT_INODE_NO);//根据外存INode编号获取对应INode。如果该INode已经在内存中，返回该内存INode；
		                                                         //如果不在内存中，则将其读入内存后上锁并返回该内存INode	                                                             //文件系统根目录外存INode编号
	rootDirInode->i_count += 0xff;//引用计数                            
}


void SystemCall::Open()
{
	Inode* pInode;

	pInode = this->NameI(SystemCall::OPEN);	/* 0 = Open, not create */
	/* 没有找到相应的Inode */
	if ( NULL == pInode ) {
		return;
	}
	this->Open1(pInode, 0);
}

/*
 * 功能：创建一个新的文件
 * 效果：建立打开文件结构，内存i节点开锁 、i_count 为正数（应该是 1）
 * */
void SystemCall::Creat()
{
	Inode* pInode;
	unsigned int newACCMode = myUser.u_arg[1];
	/* 搜索目录的模式为1，表示创建；若父目录不可写，出错返回 */
	pInode = this->NameI(SystemCall::CREATE);
	/* 没有找到相应的Inode，或NameI出错 */
	if ( NULL == pInode ) {
		if(myUser.u_error)
			return;
		/* 创建Inode */
		pInode = this->MakNode( newACCMode);
		/* 创建失败 */
		if ( NULL == pInode ){
			return;
		}
		/* 
		 * 如果所希望的名字不存在，使用参数trf = 2来调用open1()。
		 * 不需要进行权限检查，因为刚刚建立的文件的权限和传入参数mode
		 * 所表示的权限内容是一样的。
		 */
		this->Open1(pInode, 2);
	}
	else {
		/* 如果NameI()搜索到已经存在要创建的文件，则清空该文件（用算法ITrunc()）。UID没有改变
		 * 原来UNIX的设计是这样：文件看上去就像新建的文件一样。然而，新文件所有者和许可权方式没变。
		 * 也就是说creat指定的RWX比特无效。
		 * 邓蓉认为这是不合理的，应该改变。
		 * 现在的实现：creat指定的RWX比特有效 */
		this->Open1(pInode, 1);
		pInode->i_mode |= newACCMode;
	}
}

/* 
* trf == 0由open调用
* trf == 1由creat调用，creat文件的时候搜索到同文件名的文件
* trf == 2由creat调用，creat文件的时候未搜索到同文件名的文件，这是文件创建时更一般的情况
* mode参数：打开文件模式，表示文件操作是 读、写还是读写
*/
void SystemCall::Open1(Inode* pInode, int trf)
{
	/* 在creat文件的时候搜索到同文件名的文件，释放该文件所占据的所有盘块 */
	if ( 1 == trf )	{
		pInode->ITrunc();
	}

	/* 解锁inode! 
	 * 线性目录搜索涉及大量的磁盘读写操作，期间进程会入睡。
	 * 因此，进程必须上锁操作涉及的i节点。这就是NameI中执行的IGet上锁操作。
	 * 行至此，后续不再有可能会引起进程切换的操作，可以解锁i节点。
	 */

	/* 分配打开文件控制块File结构 */

	File* pFile = myOpenFileTable.FAlloc();

	if ( NULL == pFile ){
		myInodeTable.IPut(pInode);
		return;
	}

	/* 设置打开文件方式，建立File结构和内存Inode的勾连关系 */
	pFile->f_flag = (File::FREAD | File::FWRITE);
	pFile->f_inode = pInode;

	/* 无 特殊设备打开函数 */

	/* 为打开或者创建文件的各种资源都已成功分配，函数返回 */
	if ( myUser.u_error == 0 ) {
		return;
	}
	else {
		/* 释放打开文件描述符 */
		int fd = myUser.u_ar0[User::EAX];
		if(fd != -1)
		{
			myUser.u_ofiles.SetF(fd, NULL);
			/* 递减File结构和Inode的引用计数 ,File结构没有锁 f_count为0就是释放File结构了*/
			pFile->f_count--;
		}
		myInodeTable.IPut(pInode);
	}

}

void SystemCall::Close()
{
	int fd = myUser.u_arg[0];

	/* 获取打开文件控制块File结构 */
	File* pFile = myUser.u_ofiles.GetF(fd);
	if ( NULL == pFile )
	{
		return;
	}

	/* 释放打开文件描述符fd，递减File结构引用计数 */
	myUser.u_ofiles.SetF(fd, NULL);
	myOpenFileTable.CloseF(pFile);
}

void SystemCall::Seek()
{
	File* pFile;
	int fd = myUser.u_arg[0];

	pFile = myUser.u_ofiles.GetF(fd);
	if ( NULL == pFile ){
		return;  /* 若FILE不存在，GetF有设出错码 */
	}

	int offset = myUser.u_arg[1];

	/* 如果myUser.u_arg[2]在3 ~ 5之间，那么长度单位由字节变为512字节 */
	if ( myUser.u_arg[2] > 2 ) {
		offset = offset << 9;
		myUser.u_arg[2] -= 3;
	}

	switch ( myUser.u_arg[2] )
	{
		/* 读写位置设置为offset */
		case 0:
			pFile->f_offset = offset;
			break;
		/* 读写位置加offset(可正可负) */
		case 1:
			pFile->f_offset += offset;
			break;
		/* 读写位置调整为文件长度加offset */
		case 2:
			pFile->f_offset = pFile->f_inode->i_size + offset;
			break;
	}
	cout << "Successfully moved file pointer to " << pFile->f_offset << endl;
}

void SystemCall::Read()
{
	/* 直接调用Rdwr()函数即可 */
	this->Rdwr(File::FREAD);
}

void SystemCall::Write()
{
	/* 直接调用Rdwr()函数即可 */
	this->Rdwr(File::FWRITE);
}


void SystemCall::Rdwr( enum File::FileFlags mode )
{
	File* pFile;

	/* 根据Read()/Write()的系统调用参数fd获取打开文件控制块结构 */
	pFile = myUser.u_ofiles.GetF(myUser.u_arg[0]);	/* fd */
	if ( NULL == pFile ){
		/* 不存在该打开文件，GetF已经设置过出错码，所以这里不需要再设置了 */
		/*	myUser.u_error = User::EBADF;	*/
		return;
	}

	myUser.u_IOParam.m_Base = (unsigned char *)myUser.u_arg[1];	
	myUser.u_IOParam.m_Count = myUser.u_arg[2];		/* 要求读/写的字节数 */

	/* 普通文件读写 ，或读写特殊文件。对文件实施互斥访问，互斥的粒度：每次系统调用。
	为此Inode类需要增加两个方法：NFlock()、NFrele()。
	这不是V6的设计。read、write系统调用对内存i节点上锁是为了给实施IO的进程提供一致的文件视图。*/
	
	/* 设置文件起始读位置 */
	myUser.u_IOParam.m_Offset = pFile->f_offset;
	if ( File::FREAD == mode ){
		pFile->f_inode->ReadI();
	}
	else{
		pFile->f_inode->WriteI();
	}

	/* 根据读写字数，移动文件读写偏移指针 */
	pFile->f_offset += (myUser.u_arg[2] - myUser.u_IOParam.m_Count);
	

	/* 返回实际读写的字节数，修改存放系统调用返回值的核心栈单元 */
	myUser.u_ar0[User::EAX] = myUser.u_arg[2] - myUser.u_IOParam.m_Count;
}



/* 返回NULL表示目录搜索失败，否则是根指针，指向文件的内存打开i节点 ，上锁的内存i节点  */
Inode* SystemCall::NameI( enum DirectorySearchMode mode )
{
	Inode* pInode;
	Buf* pBuf;
	int freeEntryOffset;	/* 以创建文件模式搜索目录时，记录空闲目录项的偏移量 */
	unsigned int index = 0, nindex = 0;
	/* 
	 * 如果该路径是'/'开头的，从根目录开始搜索，
	 * 否则从进程当前工作目录开始搜索。
	 */
	pInode = myUser.u_cdir;
	if ( '/' == myUser.dirp[0] ){
		pInode = this->rootDirInode;
		nindex = ++index + 1;
	}
	/* 外层循环每次处理pathname中一段路径分量 */
	while (true) {
		/* 如果出错则释放当前搜索到的目录文件Inode，并退出 */
		if ( myUser.u_error != User::U_NOERROR ){
			break;	/* goto out; */
		}

		/* 整个路径搜索完毕，返回相应Inode指针。目录搜索成功返回。 */
		if ( nindex >= myUser.dirp.length() ) {
			return pInode;
		}
		/* 如果要进行搜索的不是目录文件，释放相关Inode资源则退出 */
		if ( (pInode->i_mode & Inode::IFMT) != Inode::IFDIR ) {
			myUser.u_error = User::U_ENOTDIR;
			break;	/* goto out; */
		}
		nindex = myUser.dirp.find_first_of('/', index);
		memset(myUser.u_dbuf, 0, sizeof(myUser.u_dbuf));
		memcpy(myUser.u_dbuf, myUser.dirp.data() + index, (nindex == (unsigned int)string::npos ? myUser.dirp.length() : nindex) - index);
		index = nindex + 1;
		
		/* 内层循环部分对于myUser.u_dbuf[]中的路径名分量，逐个搜寻匹配的目录项 */
		myUser.u_IOParam.m_Offset = 0;
		/* 设置为目录项个数 ，含空白的目录项*/
		myUser.u_IOParam.m_Count = pInode->i_size / (DirectoryEntry::DIRSIZ + 4);
		freeEntryOffset = 0;
		pBuf = NULL;
		while (true){
			/* 对目录项已经搜索完毕 */
			if ( 0 == myUser.u_IOParam.m_Count ){
				if ( NULL != pBuf )	{
					myBufferManager.Brelse(pBuf);
				}
				/* 如果是创建新文件 */
				if ( SystemCall::CREATE == mode && nindex >= myUser.dirp.length() ){
					/* 将父目录Inode指针保存起来，以后写目录项WriteDir()函数会用到 */
					myUser.u_pdir = pInode;
					/* 此变量存放了空闲目录项位于目录文件中的偏移量 */
					if ( freeEntryOffset )	{
						/* 将空闲目录项偏移量存入u区中，写目录项WriteDir()会用到 */
						myUser.u_IOParam.m_Offset = freeEntryOffset - (DirectoryEntry::DIRSIZ + 4);
					}
					else {
						pInode->i_flag |= Inode::IUPD;
					}
					/* 找到可以写入的空闲目录项位置，NameI()函数返回 */
					return NULL;
				}
				
				/* 目录项搜索完毕而没有找到匹配项，释放相关Inode资源，并推出 */
				myUser.u_error = User::U_ENOENT;
				goto out;
			}

			/* 已读完目录文件的当前盘块，需要读入下一目录项数据盘块 */
			if ( 0 == myUser.u_IOParam.m_Offset % Inode::BLOCK_SIZE ) {
				if ( NULL != pBuf )	{
					myBufferManager.Brelse(pBuf);
				}
				/* 计算要读的物理盘块号 */
				int phyBlkno = pInode->Bmap(myUser.u_IOParam.m_Offset / Inode::BLOCK_SIZE );
				pBuf = myBufferManager.Bread(phyBlkno );
			}

			/* 没有读完当前目录项盘块，则读取下一目录项至myUser.u_dent */
			memcpy(&myUser.u_dent, pBuf->b_addr + (myUser.u_IOParam.m_Offset % Inode::BLOCK_SIZE) , sizeof(myUser.u_dent) );
			myUser.u_IOParam.m_Offset += sizeof(DirectoryEntry);
			myUser.u_IOParam.m_Count--;

			/* 如果是空闲目录项，记录该项位于目录文件中偏移量 */
			if ( 0 == myUser.u_dent.m_ino ) {
				if ( 0 == freeEntryOffset )
					freeEntryOffset = myUser.u_IOParam.m_Offset;
				
				/* 跳过空闲目录项，继续比较下一目录项 */
				continue;
			}
			if (!memcmp(myUser.u_dbuf, &myUser.u_dent.m_name, sizeof(DirectoryEntry) - 4))
				break;
		}

		/* 
		 * 从内层目录项匹配循环跳至此处，说明pathname中
		 * 当前路径分量匹配成功了，还需匹配pathname中下一路径
		 * 分量，直至遇到'\0'结束。
		 */
		if ( NULL != pBuf )
		{
			myBufferManager.Brelse(pBuf);
		}

		/* 如果是删除操作，则返回父目录Inode，而要删除文件的Inode号在myUser.u_dent.m_ino中 */
		if ( SystemCall::DELETE == mode && nindex >= myUser.dirp.length()) {
			return pInode;
		}

		/* 
		 * 匹配目录项成功，则释放当前目录Inode，根据匹配成功的
		 * 目录项m_ino字段获取相应下一级目录或文件的Inode。
		 */
		myInodeTable.IPut(pInode);
		pInode = myInodeTable.IGet(myUser.u_dent.m_ino);
		/* 回到外层While(true)循环，继续匹配Pathname中下一路径分量 */

		if ( NULL == pInode )	/* 获取失败 */
		{
			return NULL;
		}
	}
out:
	myInodeTable.IPut(pInode);
	return NULL;
}



/* 由creat调用。
 * 为新创建的文件写新的i节点和新的目录项
 * 返回的pInode是上了锁的内存i节点，其中的i_count是 1。
 *
 * 在程序的最后会调用 WriteDir，在这里把属于自己的目录项写进父目录，修改父目录文件的i节点 、将其写回磁盘。
 *
 */
Inode* SystemCall::MakNode( int mode )
{
	Inode* pInode;

	/* 分配一个空闲DiskInode，里面内容已全部清空 */
	pInode = myFileSystem.IAlloc();
	if( NULL ==	pInode )
	{
		return NULL;
	}

	pInode->i_flag |= (Inode::IACC | Inode::IUPD);
	pInode->i_mode = mode | Inode::IALLOC;
	pInode->i_nlink = 1;

	/* 将目录项写入myUser.u_dent，随后写入目录文件 */
	this->WriteDir(pInode);
	return pInode;
}

void SystemCall::WriteDir( Inode* pInode )
{
	/* 设置目录项中Inode编号部分 */
	myUser.u_dent.m_ino = pInode->i_number;

	/* 设置目录项中pathname分量部分 */
	for ( int i = 0; i < DirectoryEntry::DIRSIZ; i++ )
	{
		myUser.u_dent.m_name[i] = myUser.u_dbuf[i];
	}

	myUser.u_IOParam.m_Count = DirectoryEntry::DIRSIZ + 4;
	myUser.u_IOParam.m_Base = (unsigned char *)&myUser.u_dent;

	/* 将目录项写入父目录文件 */
	myUser.u_pdir->WriteI();
	myInodeTable.IPut(myUser.u_pdir);
}


void SystemCall::UnLink()
{
	Inode* pInode;
	Inode* pDeleteInode;

	pDeleteInode = this->NameI(SystemCall::DELETE);
	if ( NULL == pDeleteInode )
	{
		return;
	}

	pInode = myInodeTable.IGet( myUser.u_dent.m_ino);
	if ( NULL == pInode ) {
		return;
	}
	/* 写入清零后的目录项 */
	myUser.u_IOParam.m_Offset -= (DirectoryEntry::DIRSIZ + 4);
	myUser.u_IOParam.m_Base = (unsigned char *)&myUser.u_dent;
	myUser.u_IOParam.m_Count = DirectoryEntry::DIRSIZ + 4;
	
	myUser.u_dent.m_ino = 0;
	pDeleteInode->WriteI();

	/* 修改inode项 */
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
	/* 搜索到的文件不是目录文件 */
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

void SystemCall::Rename(string ori, string cur)
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

		DirectoryEntry tmp;
		memcpy(&tmp, pBuf->b_addr + (myUser.u_IOParam.m_Offset % Inode::BLOCK_SIZE), sizeof(myUser.u_dent));

		if (strcmp(tmp.m_name, ori.c_str()) == 0) {
			strcpy(tmp.m_name, cur.c_str());
			memcpy(pBuf->b_addr + (myUser.u_IOParam.m_Offset % Inode::BLOCK_SIZE), &tmp, sizeof(myUser.u_dent));
		}
		myUser.u_IOParam.m_Offset += sizeof(DirectoryEntry);
		myUser.u_IOParam.m_Count--;
	}

	if (pBuf)
		myBufferManager.Brelse(pBuf);
}

