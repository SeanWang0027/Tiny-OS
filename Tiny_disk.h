#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <iostream>


using namespace std;
const char diskFileName[] = "myDisk.img";  //磁盘镜像文件名

class Disk {
public:
	FILE* diskPointer;                     //磁盘文件指针

public:
	Disk();
	~Disk(); 
	void Reset();
	//写磁盘函数
	void write(const unsigned char* in_buffer, unsigned int in_size, int offset = -1, unsigned int origin = SEEK_SET);
	//读磁盘函数
	void read(unsigned char* out_buffer, unsigned int out_size, int offset = -1, unsigned int origin = SEEK_SET);
	//检查镜像文件是否存在
	bool Exists();
	//打开镜像文件
	void Construct();
};
