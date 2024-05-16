#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include "Tiny_file.h"
#include "Tiny_fileSystem.h"

class OpenFileTable 
{
public:
	static const int NFILE = 100;        // 打开文件控制块File结构的数量

public:
	File m_File[NFILE];            //系统打开文件表，为所有进程共享，进程打开
	                                         //文件描述符表中包含指向打开文件表中对应File结构的指针
public:
	void Reset();
	File* FAlloc();                          //在系统打开文件表中分配一个空闲的File结构
	void CloseF(File* pFile);                //对打开文件控制块File结构的引用计数count减1，若引用计数count为0，则释放File结构
};




class InodeTable 
{
public:
	static const int NINODE = 100;           //内存Inode的数量

	InodeTable();
	Inode* IGet(int inumber);                //根据外存Inode编号获取对应Inode。如果该Inode已经在内存中，返回该内存Inode；
		                                     //如果不在内存中，则将其读入内存后上锁并返回该内存Inode
	void IPut(Inode* pNode);                 //减少该内存Inode的引用计数，如果此Inode已经没有目录项指向它，
		                                     //且无进程引用该Inode，则释放此文件占用的磁盘块
	void UpdateInodeTable();                 //将所有被修改过的内存Inode更新到对应外存Inode中
	int IsLoaded(int inumber);               //检查编号为inumber的外存Inode是否有内存拷贝，
		                                     //如果有则返回该内存Inode在内存Inode表中的索引
	Inode* GetFreeInode();                   //在内存Inode表中寻找一个空闲的内存Inode
	void Reset();

private:
	Inode m_Inode[NINODE];              //内存Inode数组，每个打开文件都会占用一个内存Inode
};


