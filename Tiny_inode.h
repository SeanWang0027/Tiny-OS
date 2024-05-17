#pragma once
#include <string>
#include "Tiny_cache.h"
#include "Tiny_buf.h"
#include <ctime>

class Inode 
{
public:
	enum INodeFlag
	{
		ILOCK = 0x1,		/* Index node locked */
		IUPD  = 0x2,		/* In-memory inode modified, needs to update corresponding on-disk inode */
		IACC  = 0x4,		/* In-memory inode accessed, needs to update last access time */
		IMOUNT = 0x8,		/* In-memory inode used to mount a sub-filesystem */
		IWANT = 0x10,		/* A process is waiting for the inode to be unlocked, needs to wake up when ILOCK is cleared */
		ITEXT = 0x20		/* In-memory inode corresponds to the text segment of a process image */
	};
    static const unsigned int IALLOC = 0x8000;		/* File in use */
	static const unsigned int IFMT = 0x6000;		/* File type mask */
	static const unsigned int IFDIR = 0x4000;		/* File type: directory */
	static const unsigned int IFCHR = 0x2000;		/* Character device special file */
	static const unsigned int IFBLK = 0x6000;		/* Block device special file, 0 indicates a regular data file */
	static const unsigned int ILARG = 0x1000;		/* File size type: large or huge file */
	static const unsigned int ISUID = 0x800;		/* Set user ID on execution */
	static const unsigned int ISGID = 0x400;		/* Set group ID on execution */
	static const unsigned int ISVTX = 0x200;		/* Text segment remains in swap area after use */
	static const unsigned int IREAD = 0x100;		/* Read permission */
	static const unsigned int IWRITE = 0x80;		/* Write permission */
	static const unsigned int IEXEC = 0x40;			/* Execute permission */
    static const unsigned int IRWXU = (IREAD|IWRITE|IEXEC);		/* Owner read, write, and execute permissions */
	static const unsigned int IRWXG = ((IRWXU) >> 3);			/* Group read, write, and execute permissions */
	static const unsigned int IRWXO = ((IRWXU) >> 6);			/* Others read, write, and execute permissions */
    
    static const int BLOCK_SIZE = 512;		/* File logical block size: 512 bytes */
	static const int ADDRESS_PER_INDEX_BLOCK = BLOCK_SIZE / sizeof(int);	/* Number of physical block numbers in an indirect index table (or index block) */

	static const int SMALL_FILE_BLOCK = 6;	/* Small file: maximum logical blocks addressed by direct index table */
	static const int LARGE_FILE_BLOCK = 128 * 2 + 6;	/* Large file: maximum logical blocks addressed by a single indirect index table */
	static const int HUGE_FILE_BLOCK = 128 * 128 * 2 + 128 * 2 + 6;	/* Huge file: maximum logical blocks addressed by double indirect indexing */
    
	static const int PIPSIZ = SMALL_FILE_BLOCK * BLOCK_SIZE;
    static int rablock;	

    short i_dev;	/* Device number */
    int i_number = -1;	/* Inode number */

    unsigned int i_flag = 0;	/* Inode flags */
    int i_count = 0;	/* Reference count */

    int i_lastr = -1;	/* Last logical block read */

    unsigned int i_mode = 0;	/* Access mode */
    int i_nlink = 0;	/* Number of links */
    short i_uid = -1;	/* User ID of owner */
    short i_gid = -1;	/* Group ID of owner */
    int i_size = 0;	/* File size */
    int i_addr[10];	/* Block addresses */
    
    Inode();	/* Constructor */
    ~Inode();	/* Destructor */
    void Reset();	/* Reset inode */

    void ReadI();	/* Read inode */
    void WriteI();	/* Write inode */
    int Bmap(int lbn);	/* Map logical block number to physical block number */
    void IUpdate(int time);	/* Update inode with current time */
    void ITrunc();	/* Truncate inode */
    void Clean();	/* Clean inode */
    void ICopy(Buf* bp, int inumber);	/* Copy inode from buffer */
};
