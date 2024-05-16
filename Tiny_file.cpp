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
    for (int i = 0; i <  OpenFiles::NOFILES; i++)
		//进程打开文件描述符表中找到空闲项，则返回之
		if (!processOpenFileTable[i]) {
			myUser.u_ar0[User::EAX] = i;
			return i;
		}

	myUser.u_ar0[User::EAX] = -1; //Open1，需要一个标志。当打开文件结构创建失败时，可以回收系统资源
	myUser.u_error = User::U_EMFILE;
	return -1;
	
}


File* OpenFiles::GetF(int fd)
{
	File* pFile;	
	/* 如果打开文件描述符的值超出了范围 */
	if(fd < 0 || fd >= OpenFiles::NOFILES) {
		myUser.u_error = User::U_EBADF;
		return NULL;
	}

	pFile = this->processOpenFileTable[fd];
	if(pFile == NULL){
		myUser.u_error = User::U_EBADF;
	}
	return pFile;	/* 即使pFile==NULL也返回它，由调用GetF的函数来判断返回值 */
}

void OpenFiles::SetF(int fd, File* pFile)
{
	if(fd < 0 || fd >= OpenFiles::NOFILES)
	{
		return;
	}
	/* 进程打开文件描述符指向系统打开文件表中相应的File结构 */
	this->processOpenFileTable[fd] = pFile;
}

void OpenFiles::Reset()
{
    memset(processOpenFileTable, 0, sizeof(processOpenFileTable));
}

/*==============================class IOParameter===================================*/
IOParameter::IOParameter()
{
	this->m_Base = 0;
	this->m_Count = 0;
	this->m_Offset = 0;
}






