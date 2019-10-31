//
//  file.cpp
//  Server
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//

#ifdef DEBUG
#include <iostream>
#endif

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "exceptions.h"
#include "file.h"
#include "read-write-helper.h"

extern std::mutex g_mtx;

using namespace EmersonClientServerFileSystem;

File::File(const string& in_file_path, int in_flags) : m_flags(in_flags)
{
    std::lock_guard<std::mutex> global_grd(g_mtx);
    
    m_fd = open(in_file_path.c_str(), in_flags, 0777);
    
    if (-1 == m_fd)
    {
        throw typename Exception::ErrorOpeningFile();
    }
    else
    {
#ifdef DEBUG
        std::cout << "opened file descriptor " << m_fd << std::endl;
#endif
    }
}

File::~File()
{
    if (O_WRONLY == (O_WRONLY & m_flags) || O_RDWR == (O_RDWR & m_flags))
    {
        fsync(m_fd);
        
        fsync(m_fd);
    }
    
    std::lock_guard<std::mutex> global_grd(g_mtx);
    
    if (close(m_fd) < 0)
    {
#ifdef DEBUG
        perror("Error closing file descriptor");
#endif
    }
    else
    {
#ifdef DEBUG
        std::cout << "closing file descriptor " << m_fd << std::endl;
#endif
    }
}

bool File::fileExists(const string& in_file_path)
{
    struct stat statbuf;
    
    return 0 == stat(in_file_path.c_str(), &statbuf);
}

long long File::getFileSize(const string& in_file_path)
{
    struct stat statbuf;
    
    bool exists = 0 == stat(in_file_path.c_str(), &statbuf);
    
    return exists? statbuf.st_size : 0;
}

long long File::getFileSize()
{
    struct stat statbuf;
    
    if (-1 == fstat(m_fd, &statbuf))
    {
        perror("Error retrieving file size");
        
        exit(EXIT_FAILURE);
    }
    
    return statbuf.st_size;
}

File::string File::read()
{
    if (O_RDONLY == (O_RDONLY & m_flags) || O_RDWR == (O_RDWR & m_flags))
    {
        int buffer_len = static_cast<int>(getFileSize());
        
        char buffer[buffer_len + 1]; // add 1 for null terminator
        
        bzero(buffer, buffer_len + 1);
        
        if (ReadWriteHelper::readFileDescriptor(m_fd, buffer, buffer_len) < 0)
        {
            throw typename Exception::ErrorReadingFromFile();
        }
        
        return string(buffer);
    }
    else
    {
        perror("Error read flag not set in File instance");
        
        exit(EXIT_FAILURE);
    }
}

void File::write(const string& in_buffer_str)
{
    if (O_WRONLY == (O_WRONLY & m_flags) || O_RDWR == (O_RDWR & m_flags))
    {
        if (ReadWriteHelper::writeFileDescriptor(m_fd, in_buffer_str.c_str(), in_buffer_str.length()) < 0)
        {
            throw typename Exception::ErrorWritingToFile();
        }
    }
    else
    {
        perror("Error write flag not set in File instance");
        
        exit(EXIT_FAILURE);
    }
}
