#pragma once

class DirectoryEntry 
{
public:
    static const int DIRSIZ = 28; // Maximum length of the file name, including null terminator
    int m_ino;                    // Inode number associated with the file
    char m_name[DIRSIZ];          // File name
};
