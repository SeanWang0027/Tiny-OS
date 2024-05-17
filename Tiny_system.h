#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include "Tiny_fileSystem.h"
#include "Tiny_inode.h"
#include "Tiny_openFileManager.h"
using namespace std;

class SystemCall
{
public:
	// Directory search modes for the NameI() function
	enum DirectorySearchMode
	{
		OPEN = 0,   // Search directory in open file mode
		CREATE = 1, // Search directory in create file mode
		DELETE = 2  // Search directory in delete file mode
	};

public:
	Inode* rootDirInode; // Root directory in-memory Inode

public:
	SystemCall();
	void Open();                          // Process the Open() system call
	void Creat();                         // Process the Creat() system call
	void Open_(Inode* pInode, int trf);   // Common part of the Open() and Creat() system calls
	void Close();                         // Process the Close() system call
	void Seek();                          // Process the Seek() system call
	void Read();                          // Process the Read() system call
	void Write();                         // Process the Write() system call
	void Rdwr(enum File::FileFlags mode); // Common part of the read and write system calls
	Inode* NameI(enum DirectorySearchMode mode); // Directory search, converting a path to the corresponding locked Inode
	Inode* MakNode(int mode);             // Used by the Creat() system call to allocate kernel resources for creating a new file
	void UnLink();                        // Unlink a file
	void WriteDir(Inode* pInode);         // Write a directory entry to the parent directory's directory file
	void ChDir();                         // Change the current working directory
	void Ls();                            // List the file entries of the current Inode
	void Rename(string ori, string cur);  // Rename a file
};
