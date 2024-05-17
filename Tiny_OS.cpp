#include <iostream>
#include <string>
#include "Tiny_system.h"
#include "Tiny_user.h"
#include "Tiny_Superblock.h"
using namespace std;
Disk myDisk;
BufferManager myBufferManager;
OpenFileTable myOpenFileTable;
SuperBlock mySuperBlock;
FileSystem myFileSystem;
InodeTable myInodeTable;
SystemCall mySystemCall;
User myUser;

void welcome()
{
    cout << "Tiny OS, version 1.0.0-release (arm64-apple-darwin23)"<<endl;
	cout << "These shell commands are defined internally.  Type `help' to see this list."<<endl;
}

void help()
{
	cout << "-fformat                          : Format a disk" << endl
		 << "-ls                               : See the content of current directory" << endl
		 << "-mkdir <param>                    : Make a New Directory" << endl
		 << "-cd <param>                       : Change the current Directory" << endl
		 << "-fcreat <param>                   : Create a File" << endl
		 << "-fopen <param>                    : Open a File" << endl
   		 << "-fclose <param>                   : Close a File named <param>" << endl
		 << "-fwrite <param1> <param2> <param3>: Input from <param2>, and write <param3> bytes in <param1>" << endl
		 << "-fread <param1> <param2> <param3> : Input from <param1>, and write to <param2> with <param3> bytes" << endl
		 << "-fread <param1> std <param2>      : Read from <param1> with <param2>" << endl
		 << "-fseek <param1> <param2> <param3> : using <param3> mode to make <param1> move <param2> steps" << endl
		 << "-fdelete <filename>               : Delete the file" << endl
		 << "-quit                             : Quit the system" << endl;
}

bool exec_args(string cmd)
{
	if(cmd == "fformat"){
		myOpenFileTable.Reset();
		myInodeTable.Reset();
		myBufferManager.Format();
		myFileSystem.FormatDevice();
		cout << "Format the disk successfully, please restart the kernel." << endl;
		return true;
	}
	else if(cmd == "ls"){
		myUser.ls();
	}
	else if(cmd == "mkdir"){
	    string dirname;
	    cin >> dirname;
		if (dirname[0] != '/'){
			dirname = myUser.curDirPath + dirname;
		}
	    myUser.mkdir(dirname);
	}
	else if (cmd == "cd"){
		string dirname;
		cin >> dirname;
	    myUser.cd(dirname);
	}
	else if (cmd == "fcreat"){
	    string filename;
		cin >> filename;
		if (filename[0] != '/'){
			filename = myUser.curDirPath + filename;
		}
		myUser.fcreate(filename);
	}
	else if (cmd == "fopen"){
	    string filename;
		cin >> filename;
		if (filename[0] != '/')
			filename = myUser.curDirPath + filename;
		
		myUser.fopen(filename);
	}
	else if (cmd == "fclose"){
	    string fd;
		cin >> fd;
		myUser.fclose(fd);
	}
	else if (cmd == "fwrite"){
	    string fd, infile, size;
		cin >> fd >> infile >> size;
		myUser.fwrite(fd, infile, size);
	}
	else if (cmd == "fread"){
	    string fd, infile, size;
		cin >> fd >> infile >> size;
		myUser.fread(fd, infile, size);
	}
	else if (cmd == "fseek"){
	    string fd, step, mode;
		cin >> fd >> step >> mode;
		myUser.fseek(fd, step, mode);
	}
	else if (cmd == "fdelete"){
	    string filename;
		cin >> filename;
		if (filename[0] != '/')
			filename = myUser.curDirPath + filename;
		myUser.fdelete(filename);
	}
	else if (cmd == "help"){
		help();
	}
	else{
		return false;
	}
	return true;
}


int main(int argc, char* argv[])
{
	welcome();
	while(true){
		cout << "Tiny-OS$";
		string cmd;
		cin >> cmd;
		if (cmd == "quit")
			break;
		else{
			if(!exec_args(cmd)){
				cout << "Invalid Command:" << cmd << endl;
			}
			if(cmd == "fformat")
				break;
		}
	}
    return 0;
}