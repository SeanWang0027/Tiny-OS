#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include "Tiny_fileSystem.h"
#include "Tiny_inode.h"
#include "Tiny_openFileManager.h"
using namespace std;

class SystemCall
{
public:
	//目录搜索模式，用于NameI()函数
	enum DirectorySearchMode
	{
		OPEN = 0,   //以打开文件方式搜索目录
		CREATE = 1, //以新建文件方式搜索目录
		DELETE = 2  //以删除文件方式搜索目录
	};

public:

	Inode* rootDirInode;         //根目录内存Inode

public:
	SystemCall();
	void Open();                          //Open()系统调用处理过程
	void Creat();                         //Creat()系统调用处理过程
	void Open1(Inode* pInode, int trf);   //Open()、Creat()系统调用的公共部分
	void Close();                         //Close()系统调用处理过程                  
	void Seek();                          //Seek()系统调用处理过程
	void Read();                          //Read()系统调用处理过程
	void Write();                         //Write()系统调用处理过程
	void Rdwr(enum File::FileFlags mode); //读写系统调用公共部分代码
	Inode* NameI(enum DirectorySearchMode mode);//目录搜索，将路径转化为相应的Inode返回上锁后的Inode
	Inode* MakNode(int mode);             //被Creat()系统调用使用，用于为创建新文件分配内核资源
	void UnLink();                        //取消文件
	void WriteDir(Inode* pInode);         //向父目录的目录文件写入一个目录项
	void ChDir();                         //改变当前工作目录
	void Ls();                            //列出当前Inode节点的文件项
	void Rename(string ori, string cur);  //重命名文件、文件夹
};
