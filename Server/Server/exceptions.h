//
//  exceptions.h
//  Server
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//

#ifndef exceptions_h
#define exceptions_h

#include <exception>

namespace EmersonClientServerFileSystem
{
    struct Exception
    {
        
    private:
        
        using exception = std::exception;
        
    public:
        
        struct ErrorOpeningFile : public exception {};
        
        struct ErrorWritingToFile : public exception {};
        
        struct ErrorReadingFromFile : public exception {};
        
        struct ErrorAddingFileAttributes : public exception {};
        
    };
}

#endif /* exceptions_h */
