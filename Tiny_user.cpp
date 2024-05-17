#include "Tiny_system.h"
#include "Tiny_user.h"
#include "Tiny_DirectoryEntry.h"
#include <iostream>
#include <fstream>
#include <cstdint>
using namespace std;

extern SystemCall mySystemCall;

User::User() 
{
	u_error = U_NOERROR;  // Initialize error code

	dirp = "/";           // Pointer to system call parameter (generally used for Pathname)
	curDirPath = "/";     // Full path of the current working directory
	u_cdir = mySystemCall.rootDirInode; // Pointer to the Inode of the current directory
	u_pdir = NULL;        // Pointer to the Inode of the parent directory
	memset(u_arg, 0, sizeof(u_arg)); // Initialize system call arguments
}

User::~User() {}

// This function changes the User object's dirp (system call parameter generally used for Pathname) data member
// Only checks if the file name is too long
bool User::checkPath(string path) 
{
	if (path.empty()) {
		cout << "Path parameter is empty" << endl;
		return false;
	}

	if (path[0] == '/' || path.substr(0, 2) != "..") {
		dirp = path;  // System call parameter (generally used for Pathname) pointer
	} else {
		if (curDirPath.back() != '/')
			curDirPath += '/';
		string pre = curDirPath;  // Full path of the current working directory, only changed by the cd command
		unsigned int p = 0;
		// Handle multiple relative paths like .. ../../
		for (; pre.length() > 3 && p < path.length() && path.substr(p, 2) == ".."; ) {
			pre.pop_back();
			pre.erase(pre.find_last_of('/') + 1);
			p += 2;
			if (p < path.length() && path[p] == '/')
				p++;
		}
		dirp = pre + path.substr(p);
	}

	if (dirp.length() > 1 && dirp.back() == '/')
		dirp.pop_back();

	for (unsigned int p = 0, q = 0; p < dirp.length(); p = q + 1) {
		q = dirp.find('/', p);
		q = min(q, (unsigned int)dirp.length());
		if (q - p > DirectoryEntry::DIRSIZ) {
			cout << "File name or directory name is too long" << endl;
			return false;
		}
	}
	return true;
}

void User::mkdir(string dirName) 
{
	if (!checkPath(dirName))
		return;
	u_arg[1] = Inode::IFDIR; // Store current system call parameter, file type: directory
	mySystemCall.Creat();
	checkError();
}

void User::ls() 
{
	ls_str.clear();
	mySystemCall.Ls();
	if (checkError())
		return;
	cout << ls_str << endl;
}

void User::__userCd__(string dirName)
{
	if (!checkPath(dirName))
		return;
	mySystemCall.ChDir();
}

void User::cd(string dirName) 
{
	if (!checkPath(dirName)) {
		return;
	}
	mySystemCall.ChDir();
	checkError();
}

void User::fcreate(string fileName)
{
	if (!checkPath(fileName))
		return;
	u_arg[1] = (Inode::IREAD | Inode::IWRITE); // Store current system call parameter
	mySystemCall.Creat();
	checkError();
}

void User::fdelete(string fileName) 
{
	if (!checkPath(fileName))
		return;
	mySystemCall.UnLink();
	checkError();
}

void User::fopen(string fileName)
{
	if (!checkPath(fileName))
		return;
	u_arg[1] = (File::FREAD | File::FWRITE); // Store current system call parameter
	mySystemCall.Open();
	if (checkError())
		return;
	cout << "Open file successfully, and the file handle is " << u_ar0[User::EAX] << endl;
}

// Pass in sfd handle
void User::fclose(string sfd) 
{
	u_arg[0] = stoi(sfd); // Store current system call parameter
	mySystemCall.Close();
	checkError();
}

void User::fseek(string sfd, string offset, string origin) 
{
	u_arg[0] = stoi(sfd);
	u_arg[1] = stoi(offset);
	if (origin == "begin") {
		u_arg[2] = 0;
	} else if (origin == "cur") {
		u_arg[2] = 1;
	} else {
		u_arg[2] = 2;
	}
	mySystemCall.Seek();
	checkError();
}

void User::fwrite(string sfd, string inFile, string size) 
{
	int fd = stoi(sfd), usize = 0;
	if (size.empty() || (usize = stoi(size)) < 0) {
		cout << "The parameter must be greater than or equal to zero!" << endl;
		return;
	}
	char* buffer = new char[usize];
	fstream fin(inFile, ios::in | ios::binary);
	if (!fin.is_open()) {
		cout << "Failed to open file " << inFile << endl;
		delete[] buffer;
		return;
	}
	fin.read(buffer, usize);
	fin.close();
	u_arg[0] = fd;
	u_arg[1] = reinterpret_cast<uintptr_t>(buffer);
	u_arg[2] = usize;
	mySystemCall.Write();

	if (checkError())
		return;
	cout << "Successfully wrote " << u_ar0[User::EAX] << " bytes" << endl;
	delete[] buffer;
}

void User::fread(string sfd, string outFile, string size) 
{
	int fd = stoi(sfd);
	int usize = stoi(size);
	char* buffer = new char[usize];
	u_arg[0] = fd;
	u_arg[1] = reinterpret_cast<uintptr_t>(buffer);
	u_arg[2] = usize;
	mySystemCall.Read();
	if (checkError())
		return;

	cout << "Successfully read " << u_ar0[User::EAX] << " bytes" << endl;
	if (outFile == "std") {
		for (int i = 0; i < u_ar0[User::EAX]; i++)
			cout << (char)buffer[i];
		cout << endl;
		delete[] buffer;
		return;
	} else {
		fstream fout(outFile, ios::out | ios::binary);
		if (!fout) {
			cout << "Failed to open file " << outFile << endl;
			delete[] buffer;
			return;
		}
		fout.write(buffer, u_ar0[User::EAX]);
		fout.close();
		delete[] buffer;
		return;
	}
}

bool User::checkError() 
{
	if (u_error != U_NOERROR) {
		switch (u_error) {
		case User::U_ENOENT:
			cout << "Cannot find the file or the folder" << endl;
			break;
		case User::U_EBADF:
			cout << "Cannot find file handle." << endl;
			break;
		case User::U_EACCES:
			cout << "Do not have access to the file" << endl;
			break;
		case User::U_ENOTDIR:
			cout << "Non-existing file" << endl;
			break;
		case User::U_ENFILE:
			cout << "Too many files" << endl;
			break;
		case User::U_EMFILE:
			cout << "Too many open files" << endl;
			break;
		case User::U_EFBIG:
			cout << "File too big" << endl;
			break;
		case User::U_ENOSPC:
			cout << "No space left on disk" << endl;
			break;
		default:
			break;
		}

		u_error = U_NOERROR;
		return true;
	}
	return false;
}