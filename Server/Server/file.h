//
//  file.h
//  Server
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// The File class is used primarily to limit the amount of code duplication that would otherwise  //
// occur in ServerBackend. It provides simplified interfaces for reading and writing files as     //
// well as a destructor that flushes writes to disk among other things.                           //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef file_h
#define file_h

#include <string>

namespace EmersonClientServerFileSystem
{
    class File
    {
        
    private:
        
        using string = std::string;
        
        int m_fd;
        
        int m_flags;
        
    public:
        
        File(const string& in_file_path, int in_flags);
        
        ~File();
        
        static bool fileExists(const string& in_file_path);
        
        static long long getFileSize(const string& in_file_path);
        
        long long getFileSize();
        
        string read();
        
        void write(const string& in_buffer_str);
        
    };
}

#endif /* File_h */
