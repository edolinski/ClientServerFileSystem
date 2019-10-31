//
//  errors.h
//  ClientServerShared
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//

#ifndef errors_h
#define errors_h

#pragma clang diagnostic ignored "-Wdocumentation"

#include <string>
#include <unordered_map>

namespace EmersonClientServerFileSystem
{
    namespace Errors
    {
        using ErrorMap = std::unordered_map/*<error code, error message>*/<int, std::string>;
        
        using ErrorMapIterator = ErrorMap::iterator;
        
        static ErrorMap messages;
        
        static ErrorMapIterator nil = end(messages);
        
        static ErrorMapIterator InvalidMessageFormat = messages.emplace(199, "InvalidMessageFormat").first;
        
        static ErrorMapIterator InvalidCommand = messages.emplace(200, "InvalidCommand").first;
        
        static ErrorMapIterator InvalidTransactionId = messages.emplace(201, "InvalidTransactionId").first;
        
        static ErrorMapIterator InvalidOperation = messages.emplace(202, "InvalidOperation").first;
        
        static ErrorMapIterator TransactionIdInUse = messages.emplace(203, "TransactionIDInUseByAnotherClient").first;
        
        static ErrorMapIterator InvalidSequenceNumber = messages.emplace(204, "New transactions must start with sequence number 0").first;
        
        static ErrorMapIterator RepeatedSequenceNumber = messages.emplace(205, "RepeatedSequenceNumber").first;
        
        static ErrorMapIterator ErrorOpeningFile = messages.emplace(206, "ErrorOpeningFile").first;
        
        static ErrorMapIterator ErrorReadingFile = messages.emplace(207, "Error reading file").first;
        
        static ErrorMapIterator ErrorWritingFile = messages.emplace(208, "Error writing file").first;
        
        static ErrorMapIterator ErrorCreatingTransaction = messages.emplace(209, "Error creating transaction").first;
        
        static ErrorMapIterator CommitWithInvalidSequenceNumber = messages.emplace(210, "Requested commit with sequence number less than maximum sequence number received").first;
        
        static ErrorMapIterator TransactionAlreadyCommitted = messages.emplace(211, "TransactionAlreadyCommitted").first;
        
        static ErrorMapIterator TransactionAborted = messages.emplace(212, "TransactionAborted").first;
        
        static inline int getErrorCode(const ErrorMapIterator& it)
        {
            return it == nil ? 0 : it->first;
        }
        
        static inline std::string getErrorMessage(const ErrorMapIterator& it)
        {
            return it == nil ? "" : it->second;
        }
    }
}

#endif
