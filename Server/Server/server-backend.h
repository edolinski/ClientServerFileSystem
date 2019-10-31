//
//  server-backend.h
//  Server
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// The ServerBackend class processes client requests including reading and writing files,         //
// generating server responses to clients, logging requests, and recovering from crash or power   //
// failure. A transaction represents a series of write requests to a given file that ends in      //
// the client either aborting or committing the transaction. When a transaction is aborted, all   //
// write requests are discarded and the file pertaining to the transaction is left unchanged or   //
// is not created. While when a transaction is committed, all write requests are flushed to disk  //
// in order of sequence number (provided the server has received a write request for each         //
// sequence number up to the maximum sequence number received). Read requests on the other hand,  //
// do not require a transaction and can be fulfilled with a single request and a single response. //
////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: periodically purge logs to ensure they remain under some threshold size

// TODO: write write requests to disk to ensure they do not exhaust memory

// TODO: serialize/deserialize write requests so don't have to ask resend on reboot write
//       requests that were already received successfully

// TODO: refactor command functions

// TODO: consider further refactoring ServerBackend, perhaps by breaking it up into other classes

#ifndef server_backend_h
#define server_backend_h

#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "errors.h"
#include "file.h"

namespace EmersonClientServerFileSystem
{
    class ServerBackend
    {
        
    private:
        
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Type Aliases                                                                           //
        // ↓                                                                                    ↓ //
        
        using atomic_bool = std::atomic_bool;
        
        using high_resolution_clock = std::chrono::high_resolution_clock;
        
        using ifstream = std::ifstream;
        
        using mutex = std::mutex;
        
        using string = std::string;
        
        using stringstream = std::stringstream;
        
        using thread = std::thread;
        
        template<class T>
        using enable_shared_from_this = std::enable_shared_from_this<T>;
        
        template<class T>
        using function = std::function<T>;
        
        template<class T>
        using lock_guard = std::lock_guard<T>;
        
        template<class T>
        using shared_ptr = std::shared_ptr<T>;
        
        template<class... Types>
        using tuple = std::tuple<Types...>;
        
        template<class T>
        using unique_lock = std::unique_lock<T>;
        
        template<class T>
        using unique_ptr = std::unique_ptr<T>;
        
        template<class K, class V>
        using unordered_map = std::unordered_map<K, V>;
        
        template<class T>
        using unordered_set = std::unordered_set<T>;
        
        template<class T>
        static constexpr auto make_shared = [](auto&&... ts) constexpr -> decltype(auto) { return std::make_shared<T>(std::forward<decltype(ts)>(ts)...);};
        
        template<class T>
        static constexpr auto make_unique = [](auto&&... ts) constexpr -> decltype(auto) { return std::make_unique<T>(std::forward<decltype(ts)>(ts)...);};
        
        static constexpr auto max = [](auto v1, auto v2) constexpr -> decltype(auto) { return std::max(v1, v2);};
        
        static constexpr auto move = [](auto t) constexpr -> decltype(auto) { return std::move(t);};
        
        static constexpr auto to_string = [](auto t) constexpr -> decltype(auto) { return std::to_string(t);};
        
        using FileName = string;
        
        using FileSize = long long;
        
        struct FileAttributes : enable_shared_from_this<FileAttributes>
        {
            FileAttributes(const FileName& in_file_name) : m_file_name(in_file_name), m_file_size(File::getFileSize(m_file_name)) {}
            const FileName m_file_name;
            FileSize m_file_size;
            mutex m_file_mtx;
        };
        
        using Command = string;
        
        using TxnId = int;
        
        using SeqNum = int;
        
        using ContentLen = int;
        
        using Data = string;
        
        using RequestTuple = tuple<Command, TxnId, SeqNum, ContentLen, Data>;
        
        using Timestamp = high_resolution_clock::time_point;
        
        // Note: A CommandFunction accepts a preparsed message as input and produces a response
        //       to send back to the client as well as whether or not the transaction is still in
        //       progress. For example, if the client sent an abort or commit request,
        //       OutTxnInProgress will be set to false assuming the request was successful.
        using CommandFunction = function<void(const RequestTuple& in_message, string& out_response, bool& out_transaction_in_progress)>;
        
        using TimerFunction = function<void(const TxnId in_txn_id, Timestamp in_prev_timestamp, const FileName in_file_name)>;
        
        using CommandMap = unordered_map<Command, CommandFunction>;
        
        using FileAttributesMap = unordered_map<FileName, FileAttributes*>;
        
        using FileNameFileSizeMap = unordered_map<FileName, FileSize>;
        
        using TxnIdFileNameMap = unordered_map<TxnId, FileName>;
        
        using BufferMap = unordered_map<SeqNum, Data>;
        
        using SharedPtrTransactionMutex = shared_ptr<mutex>;
        
        using SharedPtrFileAttributes = shared_ptr<FileAttributes>;
        
        // Note: Each transaction has its own mutex to handle multiple clients interacting with
        //       the same transaction. It has been made a shared_ptr to handle the case where one
        //       client commits/aborts the transaction while other clients are still interacting
        //       with the transaction. That is, the server will make a copy of the
        //       SharedPtrTransactionMutex to guarantee the memory for the mutex remains
        //       allocated even in the event another client has just committed/aborted the same
        //       transaction. Although such an occurrence is an error on the client-side, this
        //       ensures the server will not crash attempting to acquire a deallocated mutex.
        using TransactionAttributesTuple = tuple/*<., ., ., MaxSeqNumReceived, TimeOfMostRecentTxnUpdate>*/<SharedPtrTransactionMutex, SharedPtrFileAttributes, BufferMap, SeqNum, Timestamp>;
        
        using TransactionAttributesMap = unordered_map<TxnId, TransactionAttributesTuple>;
        
        using TransactionAttributesMapIterator = TransactionAttributesMap::iterator;
        
        using CommitSet = unordered_set<TxnId>;
        
        // ↑                                                                                    ↑ //
        // Type Aliases                                                                           //
        ////////////////////////////////////////////////////////////////////////////////////////////
        
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Member Variables                                                                       //
        // ↓                                                                                    ↓ //
        
        //Note: The m_txn_timer_function runs on a separate thread for each transaction and sits
        //      in a tight loop sleeping until the lastest transaction timestamp +
        //      Constants::transaction_timeout_seconds. It then wakes up to retreive the most
        //      recent transaction timestamp again (in case it has since changed) and checks to
        //      see if it is within Constants::transaction_timeout_seconds of the current time.
        //      If it is, the transaction has not timed out, and so the latest transaction
        //      timestamp is updated and the loop repeated. Otherwise, the transaction is
        //      terminated and the function returns. The function will also return if the
        //      transaction has terminated by some other means.
        TimerFunction m_txn_timer_function;
        
        CommandMap m_command_to_function;
        
        TransactionAttributesMap m_txn_id_to_transaction_attributes;
        
        FileAttributesMap m_file_name_to_ptr_to_file_attributes;
        
        CommitSet m_commits; // keeps track of committed transaction ids
        
        const FileName m_transaction_log = ".transactionlog.txt";
        
        const FileName m_timeout_log = ".timeoutlog.txt";
        
        const FileName m_commit_log = ".commitlog.txt";
        
        const FileName m_abort_log = ".abortlog.txt";
        
        const string m_directory;
        
        mutex m_member_mtx;
        
        atomic_bool m_initialize = ATOMIC_VAR_INIT(true);
        
        // ↑                                                                                    ↑ //
        // Member Variables                                                                       //
        ////////////////////////////////////////////////////////////////////////////////////////////
        
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Member Functions                                                                       //
        // ↓                                                                                    ↓ //
        
        // returns a string generated from the input arguments and formatted according to the
        // response protocol to be used as the server's response to the client
        string generateResponse(const char * in_command, TxnId in_txn_id, SeqNum in_seq_num, Errors::ErrorMapIterator in_error = Errors::nil, const Data& in_data = "");
        
        // returns the client request as a tuple by using the wire protocol to extract the
        // relevant fields
        RequestTuple getClientRequestAsTuple(const char * in_request_header, const char * in_request_payload = nullptr);
        
        // creates and returns shared pointer to new file attributes and associates file name
        // with raw pointer to these file attributes
        auto getNewFileAttributes(const FileName& in_file_name);
        
        // creates and returns a TransactionAttributesTuple
        auto getNewTransactionAttributes(SharedPtrTransactionMutex&& in_sp_txn_mtx, SharedPtrFileAttributes&& in_sp_file_attributes, Timestamp timestamp);
        
        // adds a new entry to the transaction attributes map, creating new file attributes if
        // necessary
        void addNewTransaction(TxnId in_txn_id, FileName&& in_file_name);
        
        // used for initializing the timer function and command functions
        void initializeFunctions();
        
        // Note: initializeTransactions retrieves the last known consistent state of the file
        //       system from loadFilesAndTransactions. initializeTransactions then uses this
        //       state to restart any transactions that were in progress at the time of the last
        //       system crash or power failure. initializeTransactions also passes this state to
        //       truncateFiles so any writes flushed to disk as part of any incomplete
        //       transactions will be expunged.
        void initializeTransactions();
        
        // Note: initializeLambdasAndTransactions is not called in the constructor of
        //       ServerBackend as each function's captured "this" pointer will be misaligned.
        //       Therefore, this function is called in processRequest and operates much like a
        //       singleton with double-checked locking.
        void initializeFunctionsAndTransactions();
        
        // Note: loadFilesAndTransactions is necessary on reboot to recreate the last known
        //       consistent state of the file system. This includes what transactions were in
        //       progress at the time of the last system crash or power failure, as well as the
        //       size of each file prior to the start of these interrupted transactions. In
        //       progress transactions are found by extracting transactions from
        //       m_transaction_log that are not present in m_timeout_log, m_commit_log, or
        //       m_abort_log. While each file's file size is determined by extracting
        //       the maximum log recorded file size for each file.
        void loadFilesAndTransactions(FileNameFileSizeMap& out_file_names_to_file_sizes, TxnIdFileNameMap& out_txn_ids_to_file_names);
        
        // Note: logTransaction is only called when a transaction is created, timed out,
        //       committed, or aborted.
        //
        // appends a new line to the hidden log file containing the transaction id, file name,
        // and file size each delimited by a space character
        void logTransaction(const FileName& in_log_name, const TxnId in_txn_id, const FileName& in_file_name);
        
        // extracts and validates command from in_message and defers to the associated command
        // function
        void processCommand(const RequestTuple& in_message, string& out_response, bool& out_transaction_in_progress);
        
        // Note: removeTransaction is not thread-safe so mutex protecting shared data structure
        //       m_txn_ids_to_transaction_attributes must be acquired before invocation of
        //       removeTransaction in a multithreaded environment.
        //
        // removes given iterator from the transaction attributes map
        void removeTransaction(TransactionAttributesMapIterator in_txn_it);
        
        // Note: truncateFiles is necessary on reboot in case the server crashed or lost power
        //       during one or more commit operations that had yet to complete. This ensures the
        //       file system will remain in a consistent state as any writes that were flushed to
        //       disk before the transaction completed will be expunged.
        //
        // iterates through each file in in_file_names_to_file_sizes and truncates file to
        // specified size
        void truncateFiles(const FileNameFileSizeMap& in_file_names_to_file_sizes);
        
        // updates the given timestamp to the current time
        void updateTransactionTimestamp(Timestamp& out_timestamp);
        
    public:
        
        // ctor in_directory argument corresponds to directory where files and logs are to be
        // stored
        ServerBackend(string in_directory);
        
        // returns the content length found in the request header, otherwise returns error and
        // sets the server response
        int getContentLength(const char * in_request_header, string& out_server_response_str, bool& out_transaction_in_progress);
        
        // returns the request header length according to the protocol
        int getRequestHeaderLength();
        
        // forwards request to processCommand for processing of request
        void processRequest(const char * in_request_header, const char * in_request_payload, string& out_server_response_str, bool& out_transaction_in_progress);
        
        // ↑                                                                                    ↑ //
        // Member Functions                                                                       //
        ////////////////////////////////////////////////////////////////////////////////////////////
    };
}

#endif /* server_backend_h */
