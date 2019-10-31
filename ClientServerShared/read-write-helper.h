//
//  read-write-helper.h
//  ClientServerShared
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// The ReadWriteHelper functions are used to eliminate code duplication that would otherwise      //
// occur in the Client and in the Server whenever reading or writing a socket or file descriptor. //
// Unlike the read and write system calls on which these functions are based, all reads and       //
// writes read/write the number of bytes specified as an argument or return an error.             //
// readFileDescriptor offers the additional functionality of being able to set a timeout on the   //
// read operation given a pointer to a mutex and condition variable.                              //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef read_write_helper_h
#define read_write_helper_h

#include <condition_variable>
#include <mutex>

#include <errno.h>

namespace EmersonClientServerFileSystem
{
    namespace ReadWriteHelper
    {
        static inline ssize_t readFileDescriptor(int in_fd, char * out_buffer, size_t in_buffer_len, time_t in_timeout_seconds = 0, std::mutex * in_p_fd_mtx = nullptr, std::condition_variable * in_p_fd_cv = nullptr)
        {
            assert(!(nullptr == out_buffer));
            
            size_t total_bytes_read = 0;
            
            while (total_bytes_read < in_buffer_len)
            {
                if (in_timeout_seconds > 0)
                {
                    assert(!(nullptr == in_p_fd_mtx || nullptr == in_p_fd_cv));
                    
                    std::unique_lock<std::mutex> fd_lck(*in_p_fd_mtx);
                    
                    if (std::cv_status::timeout == in_p_fd_cv->wait_for(fd_lck, std::chrono::seconds(in_timeout_seconds)))
                    {
                        errno = ETIMEDOUT;
                        
                        break;
                    }
                }
                
                ssize_t bytes_read = read(in_fd, out_buffer + total_bytes_read, in_buffer_len - total_bytes_read);
                
                if (bytes_read <= 0)
                {
                    break;
                }
                
                total_bytes_read += bytes_read;
            }
            
            return total_bytes_read == in_buffer_len ? total_bytes_read : -1;
        }
        
        static inline ssize_t writeFileDescriptor(int in_fd, const char * in_buffer, size_t in_buffer_len)
        {
            assert(!(nullptr == in_buffer));
            
            assert(strlen(in_buffer) == in_buffer_len);
            
            size_t total_bytes_written = 0;
            
            while (total_bytes_written < in_buffer_len)
            {
                ssize_t bytes_written = write(in_fd, in_buffer + total_bytes_written, in_buffer_len - total_bytes_written);
                
                if (bytes_written < 0)
                {
                    break;
                }
                
                total_bytes_written += bytes_written;
            }
            
            return total_bytes_written == in_buffer_len ? total_bytes_written : -1;
        }
    }
}

#endif /* read_write_helper_h */
