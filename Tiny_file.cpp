#include "Tiny_file.h"
#include "Tiny_user.h"
extern User myUser;


File::File()
{
	return;
}

File::~File()
{
	return;
}

void File::Reset() 
{
    this->f_count = 0;
	this->f_offset = 0;
	this->f_inode = NULL;
	this->f_flag = 0;

}


OpenFiles::OpenFiles()
{
	memset(processOpenFileTable, 0, sizeof(processOpenFileTable));
}

OpenFiles::~OpenFiles()
{
}


int OpenFiles::AllocFreeSlot()
{
    for (int i = 0; i <  OpenFiles::NOFILES; i++){
		if (!processOpenFileTable[i]) {
			myUser.u_ar0[User::EAX] = i;
			return i;
		}
	}
	int test = -1;
	myUser.u_ar0[User::EAX] = test;
	myUser.u_error = User::U_EMFILE;
	return test;
}


bool OpenFiles::Judge(int fd)
{
	if(fd >= OpenFiles::NOFILES)
	{
		return false;
	}

	if (fd<0)
	{
		return false;
	}
	return true;
}

File* OpenFiles::GetF(int fd)
{
	File* pFile;	
	if (!OpenFiles::Judge(fd))
	{
		return NULL;
	}
	pFile = this->processOpenFileTable[fd];
	if(pFile == NULL){
		myUser.u_error = User::U_EBADF;
	}
	return pFile;
}

void OpenFiles::SetF(int fd, File* pFile)
{
	if (!OpenFiles::Judge(fd))
	{
		return;
	}
	this->processOpenFileTable[fd] = pFile;
}

void OpenFiles::Reset()
{
    memset(processOpenFileTable, 0, sizeof(processOpenFileTable));
}

IOParameter::IOParameter()
{
	return;
}






