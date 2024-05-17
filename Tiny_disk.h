#pragma once
#define _CRT_SECURE_NO_WARNINGS
#include <cstdio>
#include <iostream>


using namespace std;
const char diskFileName[] = "myDisk.img";  

class Disk {
public:
	FILE* diskPointer;           

public:
	Disk();
	~Disk(); 
	void Reset();

	void write(const unsigned char* in_buffer, unsigned int in_size, int offset = -1, unsigned int origin = SEEK_SET);

	void read(unsigned char* out_buffer, unsigned int out_size, int offset = -1, unsigned int origin = SEEK_SET);

	bool Exists();

	void Construct();
};
