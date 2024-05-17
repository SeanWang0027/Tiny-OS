#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include "Tiny_file.h"
#include "Tiny_fileSystem.h"

class OpenFileTable 
{
public:
	static const int NFILE = 100;        // Number of File structures in the open file control block

public:
	File m_File[NFILE];                  // System-wide open file table, shared by all processes.
	                                     // The process open file descriptor table contains pointers to
	                                     // the corresponding File structures in this table.
public:
	void Reset();
	File* FAlloc();                      // Allocate a free File structure in the system-wide open file table
	void CloseF(File* pFile);            // Decrease the reference count of the File structure. 
	                                     // If the reference count is 0, free the File structure.
};

class InodeTable 
{
public:
	static const int NINODE = 100;       // Number of in-memory Inodes

	InodeTable();
	Inode* IGet(int inumber);            // Get the corresponding Inode based on the on-disk Inode number.
	                                     // If the Inode is already in memory, return it;
	                                     // otherwise, read it into memory, lock it, and return it.
	void IPut(Inode* pNode);             // Decrease the reference count of the in-memory Inode.
	                                     // If it has no directory entries and is not referenced by any process, free its disk blocks.
	void UpdateInodeTable();             // Update all modified in-memory Inodes to their corresponding on-disk Inodes
	int IsLoaded(int inumber);           // Check if the in-memory copy of the on-disk Inode with number inumber exists,
	                                     // and if so, return its index in the in-memory Inode table
	Inode* GetFreeInode();               // Find a free in-memory Inode in the in-memory Inode table
	void Reset();

private:
	Inode m_Inode[NINODE];               // Array of in-memory Inodes, each open file occupies one in-memory Inode
};
