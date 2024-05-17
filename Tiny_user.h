#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include "Tiny_system.h"
#include "Tiny_DirectoryEntry.h"
#include <string>
#include <vector>

class User
{
public:
	static const int EAX = 0; 

	enum ErrorCode
	{
		U_NOERROR = 0,	/* No error */
		U_EPERM = 1,	/* Operation not permitted */
		U_ENOENT = 2,	/* No such file or directory */
		U_ESRCH = 3,	/* No such process */
		U_EINTR = 4,	/* Interrupted system call */
		U_EIO = 5,		/* I/O error */
		U_ENXIO = 6,	/* No such device or address */
		U_E2BIG = 7,	/* Arg list too long */
		U_ENOEXEC = 8,	/* Exec format error */
		U_EBADF = 9,	/* Bad file number */
		U_ECHILD = 10,	/* No child processes */
		U_EAGAIN = 11,	/* Try again */
		U_ENOMEM = 12,	/* Out of memory */
		U_EACCES = 13,	/* Permission denied */
		U_EFAULT = 14,	/* Bad address */
		U_ENOTBLK = 15, /* Block device required */
		U_EBUSY = 16,	/* Device or resource busy */
		U_EEXIST = 17,	/* File exists */
		U_EXDEV = 18,	/* Cross-device link */
		U_ENODEV = 19,	/* No such device */
		U_ENOTDIR = 20, /* Not a directory */
		U_EISDIR = 21,	/* Is a directory */
		U_EINVAL = 22,	/* Invalid argument */
		U_ENFILE = 23,	/* File table overflow */
		U_EMFILE = 24,	/* Too many open files */
		U_ENOTTY = 25,	/* Not a typewriter(terminal) */
		U_ETXTBSY = 26, /* Text file busy */
		U_EFBIG = 27,	/* File too large */
		U_ENOSPC = 28,	/* No space left on device */
		U_ESPIPE = 29,	/* Illegal seek */
		U_EROFS = 30,	/* Read-only file system */
		U_EMLINK = 31,	/* Too many links */
		U_EPIPE = 32,	/* Broken pipe */
		U_ENOSYS = 100,
	};

public:
	Inode *u_cdir;
	Inode *u_pdir;		
	DirectoryEntry u_dent;	
	char u_dbuf[DirectoryEntry::DIRSIZ];
	string curDirPath;				
	string dirp;			

	int u_arg[5]; 

	unsigned int u_ar0[5]; 
	ErrorCode u_error; 
	OpenFiles u_ofiles;	
	IOParameter u_IOParam;	
	string ls_str;

public:
	User();
	~User();

	void ls();
	void cd(string dirName);
	void mkdir(string dirName);
	void fcreate(string fileName);
	void fdelete(string fileName);
	void fopen(string fileName);
	void fclose(string fd);
	void fseek(string fd, string offset, string origin);
	void fwrite(string fd, string inFile, string size);
	void fread(string fd, string outFile, string size);

private:
	bool checkError();
	bool checkPath(string path);
	void __userCd__(string dirName);
};
