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
	u_error = U_NOERROR;  //存放错误码

	dirp = "/";                                   //系统调用参数(一般用于Pathname)的指针
	curDirPath = "/";                             //当前工作目录完整路径
	u_cdir = mySystemCall.rootDirInode;//指向当前目录的Inode指针
	u_pdir = NULL;                     //指向父目录的Inode指针
	memset(u_arg, 0, sizeof(u_arg));                  //指向核心栈现场保护区中EAX寄存器
}

User::~User() {}

//此函数改变User对象的dirp(系统调用参数 一般用于Pathname)数据成员
//只检查文件名是否过长
bool User::checkPath(string path) 
{
	if (path.empty()) {
		cout << "参数路径为空" << endl;
		return false;
	}

	if (path[0] == '/' || path.substr(0, 2) != "..")
		dirp = path;            //系统调用参数(一般用于Pathname)的指针
	else {
		if (curDirPath.back() != '/')
			curDirPath += '/';
		string pre = curDirPath;//当前工作目录完整路径 cd命令才会改变curDirPath的值
		unsigned int p = 0;
		//可以多重相对路径 .. ../../
		for (; pre.length() > 3 && p < path.length() && path.substr(p, 2) == ".."; ) {
			pre.pop_back();
			pre.erase(pre.find_last_of('/') + 1);
			p += 2;
			p += p < path.length() && path[p] == '/';
		}
		dirp = pre + path.substr(p);
	}

	if (dirp.length() > 1 && dirp.back() == '/')
		dirp.pop_back();

	for (unsigned int p = 0, q = 0; p < dirp.length(); p = q + 1) {
		q = dirp.find('/', p);
		q = min(q, (unsigned int)dirp.length());
		if (q - p > DirectoryEntry::DIRSIZ) {
			cout << "文件名或文件夹名过长" << endl;
			return false;
		}
	}
	return true;
}

void User::mkdir(string dirName) 
{
	if (!checkPath(dirName))
		return;
	u_arg[1] = Inode::IFDIR;//存放当前系统调用参数 文件类型：目录文件
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

void User::frename(string ori, string cur)
{
	string curDir = curDirPath;
	if (!checkPath(ori))
		return;
	if (!checkPath(cur))
		return;
	if (ori.find('/') != string::npos) {
		string nextDir = ori.substr(0, ori.find_last_of('/'));
		cd(nextDir);
		ori.substr(ori.find_last_of('/') + 1);
		if (cur.find('/') != string::npos)
		    cur= cur.substr(cur.find_last_of('/') + 1);
	}
	mySystemCall.Rename(ori, cur);
	cd(curDir);
	if (checkError())
		return;
}

void User::__userTree__(string path, int depth)
{
	vector<string> dirs;
	string nextDir;
	ls_str.clear();
	mySystemCall.Ls();
	for (int i = 0, p = 0; i < ls_str.length(); ) {
		p = ls_str.find('\n', i);
		dirs.emplace_back(ls_str.substr(i, p - i));
		i = p + 1;
	}
	for (int i = 0; i < dirs.size(); i++) {
		nextDir = (path == "/" ? "" : path) + '/' + dirs[i];
		for (int i = 0; i < depth + 1; i++)
			cout << "|   ";
		cout << "|---" << dirs[i] << endl;
		__userCd__(nextDir);
		if (u_error != User::U_NOERROR) {//访问到文件
			u_error = User::U_NOERROR;
			continue;
		}
		__userTree__(nextDir, depth + 1);
	}
	__userCd__(path);
	return;
}

void User::ftree(string path)
{
	if (curDirPath.length() > 1 && curDirPath.back() == '/')
		curDirPath.pop_back();
	string curDir = curDirPath;
	if (path == "") 
		path = curDir;

	if (!checkPath(path))
		return;

	path = dirp;
	__userCd__(path);
	if (u_error != User::U_NOERROR) {//访问到文件
		cout << "目录路径不存在！" << endl;
		u_error = User::U_NOERROR;
		__userCd__(curDir);
		return;
	}
	cout << "|---" << (path == "/" ? "/" : path.substr(path.find_last_of('/') + 1)) << endl;
	__userTree__(path, 0);
	__userCd__(curDir);
}

void User::__userCd__(string dirName)
{
	if (!checkPath(dirName))
		return;
	mySystemCall.ChDir();
}

void User::cd(string dirName) 
{
	if (!checkPath(dirName)){
        return;
    }   
	mySystemCall.ChDir();
	checkError();
}

void User::fcreate(string fileName)
{
	if (!checkPath(fileName))
		return;
	u_arg[1] = (Inode::IREAD | Inode::IWRITE);//存放当前系统调用参数
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
	u_arg[1] = (File::FREAD | File::FWRITE);//存放当前系统调用参数
	mySystemCall.Open();
	if (checkError())
		return;
	cout << "Open file successfully, and the file handle is " << u_ar0[User::EAX] << endl;
}

//传入sfd句柄
void User::fclose(string sfd) 
{
	u_arg[0] = stoi(sfd);//存放当前系统调用参数
	mySystemCall.Close();
	checkError();
}

void User::fseek(string sfd, string offset, string origin) 
{
	u_arg[0] = stoi(sfd);
	u_arg[1] = stoi(offset);
	if (origin == "begin"){
		u_arg[2] = 0;
	}
	else if (origin == "cur"){
		u_arg[2] = 1;
	}
	else{
		u_arg[2] = 2;
	}
	mySystemCall.Seek();
	checkError();
}

void User::fwrite(string sfd, string inFile, string size) 
{
	int fd = stoi(sfd), usize = 0;
	if (size.empty() || (usize = stoi(size)) < 0) {
		cout << "参数必须大于等于零 ! \n";
		return;
	}
	char* buffer = new char[usize];
	fstream fin(inFile, ios::in | ios::binary);
	if (!fin.is_open()) {
		cout << "打开文件" << inFile << "失败" << endl;
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
	cout << "成功写入" << u_ar0[User::EAX] << "字节" << endl;
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

	cout << "成功读出" << u_ar0[User::EAX] << "字节" << endl;
	if (outFile == "std") {
		for (int i = 0; i < u_ar0[User::EAX]; i++)
			cout << (char)buffer[i];
		cout << endl;
		delete[] buffer;
		return;
	}
	else {
		fstream fout(outFile, ios::out | ios::binary);
		if (!fout) {
			cout << "打开文件" << outFile << "失败" << endl;
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
			cout << "Cannot find file's handle." << endl;
			break;
		case User::U_EACCES:
			cout << "Do not have access to the file" << endl;
			break;
		case User::U_ENOTDIR:
			cout << "Non exist file" << endl;
			break;
		case User::U_ENFILE:
			cout << "Too many files" << endl;
			break;
		case User::U_EMFILE:
			cout << "Too many open files" << endl;
			break;
		case User::U_EFBIG:
			cout << "Too big file" << endl;
			break;
		case User::U_ENOSPC:
			cout << "Limit space for disk" << endl;
			break;
		default:
			break;
		}

		u_error = U_NOERROR;
		return true;
	}
	return false;
}