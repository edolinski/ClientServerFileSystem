//
//  server-backend.cpp
//  Server
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//

#include <fstream>
#ifdef DEBUG
#include <iostream>
#endif
#include <sstream>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "constants.h"
#include "exceptions.h"
#include "server-backend.h"

#define COMMAND_FUNCTION_PARAMS [this](const RequestTuple& in_client_request_tuple, string& out_server_response_str, bool& out_transaction_in_progress)
#define NOW high_resolution_clock::now()
#define START_TRANSACTION_TIMER() thread(m_txn_timer_function, in_txn_id, curr_timestamp, move(in_file_name)).detach()
#define SET_RESPONSE_3(command, txn_id, seq_num) out_server_response_str = generateResponse(command, txn_id, seq_num)
#define SET_RESPONSE_5(command, txn_id, seq_num, error, data) out_server_response_str = generateResponse(command, txn_id, seq_num, error, data)
#define SET_ERROR_AND_RETURN(error) out_transaction_in_progress = false; SET_RESPONSE_5(Constants::error_cmd, txn_id, seq_num, error, Errors::getErrorMessage(error)); return
#define SET_FORMATTING_ERROR() out_transaction_in_progress = false; SET_RESPONSE_5(Constants::error_cmd, Constants::default_txn_id, Constants::error_seq_num, Errors::InvalidMessageFormat, Errors::getErrorMessage(Errors::InvalidMessageFormat))
#define SET_ACK_AND_RETURN() SET_RESPONSE_3(Constants::ack_cmd, txn_id, seq_num); return
#define SET_NEW_TXN_AND_RETURN(txn_id) SET_RESPONSE_3(Constants::ack_cmd, txn_id, Constants::initial_seq_num); return
#define SET_READ_AND_RETURN(buffer) SET_RESPONSE_5(Constants::ack_cmd, txn_id, seq_num, Errors::nil, buffer); return
#define SET_ASK_RESEND_AND_RETURN(seq_num) SET_RESPONSE_3(Constants::ask_resend_cmd, txn_id, seq_num); return
#define RETURN_IF_INVALID_ID() if (0 == m_txn_id_to_transaction_attributes.count(txn_id)) { SET_ERROR_AND_RETURN(Errors::InvalidTransactionId); }
#define RETURN_ERROR_IF_ABORTED() if (0 == m_txn_id_to_transaction_attributes.count(txn_id)) { SET_ERROR_AND_RETURN(Errors::TransactionAborted); }
#define RETURN_ERROR_IF_COMMITTED(error) if (m_commits.count(txn_id)) { SET_ERROR_AND_RETURN(error); }
#define RETURN_ERROR_IF_COMMITTED_OR_INVALID_ID() RETURN_ERROR_IF_COMMITTED(Errors::InvalidOperation); RETURN_IF_INVALID_ID()
#define RETURN_ERROR_IF_COMMITTED_OR_ABORTED() RETURN_ERROR_IF_COMMITTED(Errors::TransactionAlreadyCommitted); RETURN_ERROR_IF_ABORTED()
#define RETURN_ACK_IF_COMMITTED() if (m_commits.count(txn_id)) { SET_ACK_AND_RETURN(); }
#define RETURN_ACK_IF_COMMITTED_OR_ERROR_IF_INVALID_ID() RETURN_ACK_IF_COMMITTED(); RETURN_IF_INVALID_ID()

#ifdef DEBUG
extern std::mutex g_mtx;
#endif

using namespace EmersonClientServerFileSystem;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Member Functions                                                                        //
// ↓                                                                                            ↓ //

ServerBackend::ServerBackend(string in_directory) : m_directory(!in_directory.empty() && !('/' == in_directory.back()) ? move(in_directory) + '/' : move(in_directory))
{
    // TODO: initialize functions in ctor (Note: this is safe as long as ServerBackend remains non-
    //       copyable and non-movable as otherwise "this" pointer is not recaptured on copy/move)
}

int ServerBackend::getContentLength(const char * in_request_header, string& out_server_response_str, bool& out_transaction_in_progress)
{
    assert(!(nullptr == in_request_header));
    
    if (Constants::wire_protocol.isValidRequestFormat(in_request_header))
    {
        auto [command, txn_id, seq_num, content_len, data] = getClientRequestAsTuple(in_request_header);
        
        return content_len;
    }
    else
    {
        SET_FORMATTING_ERROR();
        
        return -1;
    }
}

int ServerBackend::getRequestHeaderLength()
{
    return Constants::request_header_len;
}

void ServerBackend::processRequest(const char * in_request_header, const char * in_request_payload, string& out_server_response_str, bool& out_transaction_in_progress)
{
    assert(!(nullptr == in_request_header || nullptr == in_request_payload));
    
    initializeFunctionsAndTransactions();
    
    processCommand(getClientRequestAsTuple(in_request_header, in_request_payload), out_server_response_str, out_transaction_in_progress);
}

// ↑                                                                                            ↑ //
// Public Member Functions                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
// Private Member Functions                                                                       //
// ↓                                                                                            ↓ //

ServerBackend::string ServerBackend::generateResponse(const char * in_command, TxnId in_txn_id, SeqNum in_seq_num, Errors::ErrorMapIterator in_error, const Data& in_data)
{
    assert(!(nullptr == in_command));
    
    int err_code = 0;
    
    if (Errors::nil != in_error)
    {
        err_code = Errors::getErrorCode(in_error);
    }
    
    string response_header;
    
    response_header.reserve(Constants::response_header_len);
    
    response_header = string(in_command) + Constants::delimiting_character + to_string(in_txn_id) + Constants::delimiting_character + to_string(in_seq_num) + Constants::delimiting_character + to_string(err_code) + Constants::delimiting_character + to_string(in_data.length());
    
    if (response_header.length() < Constants::response_header_len)
    {
        response_header.push_back(Constants::delimiting_character);
    }
    
    response_header.append(Constants::response_header_len - response_header.length(), Constants::padding_character);
    
#ifdef DEBUG
    if (!Constants::wire_protocol.isValidResponseFormat(response_header.c_str()))
    {
        std::cerr << "Server generated header \"" << response_header << "\" is invalid" << std::endl;
    }
#endif
    
    return response_header + in_data;
}

ServerBackend::RequestTuple ServerBackend::getClientRequestAsTuple(const char * in_request_header, const char * in_request_payload)
{
    RequestTuple client_request_tuple;
    
    if (in_request_header)
    {
        Constants::wire_protocol.extractHeaderFields(in_request_header, client_request_tuple);
        
        if (in_request_payload)
        {
            auto& [command, txn_id, seq_num, content_len, data] = client_request_tuple;
            
            data = string(in_request_payload);
        }
    }
    
    return client_request_tuple;
}

auto ServerBackend::getNewFileAttributes(const FileName& in_file_name)
{
    auto [fntptfa_it, emplace_successful] = m_file_name_to_ptr_to_file_attributes.emplace(in_file_name, nullptr);
    
    if (emplace_successful)
    {
        // TODO: handle failure on new
        auto p_file_attributes = new FileAttributes(in_file_name);
        
        fntptfa_it->second = p_file_attributes;
        
        return SharedPtrFileAttributes(p_file_attributes, [this](auto p_file_attributes)
                                       {
                                           auto it = m_file_name_to_ptr_to_file_attributes.find(p_file_attributes->m_file_name);
                                           m_file_name_to_ptr_to_file_attributes.erase(it);
                                           delete p_file_attributes;
                                       });
    }
    else
    {
        throw typename Exception::ErrorAddingFileAttributes();
    }
}

auto ServerBackend::getNewTransactionAttributes(SharedPtrTransactionMutex&& in_sp_txn_mtx, SharedPtrFileAttributes&& in_sp_file_attributes, Timestamp in_timestamp)
{
    return TransactionAttributesTuple(move(in_sp_txn_mtx), move(in_sp_file_attributes), BufferMap(), Constants::initial_seq_num + 1, in_timestamp);
}

void ServerBackend::addNewTransaction(TxnId in_txn_id, FileName&& in_file_name)
{
    auto fntptfa_it = m_file_name_to_ptr_to_file_attributes.find(in_file_name);
    
    auto curr_timestamp = NOW;
    
    auto sp_txn_mtx = make_shared<mutex>();
    
    if (end(m_file_name_to_ptr_to_file_attributes) == fntptfa_it)
    {
        m_txn_id_to_transaction_attributes.emplace(in_txn_id, getNewTransactionAttributes(move(sp_txn_mtx), getNewFileAttributes(in_file_name), curr_timestamp));
    }
    else
    {
        const auto& [file_name, p_file_attributes] = *fntptfa_it;
        
        auto sp_file_attributes = p_file_attributes->shared_from_this();
        
        m_txn_id_to_transaction_attributes.emplace(in_txn_id, getNewTransactionAttributes(move(sp_txn_mtx), move(sp_file_attributes), curr_timestamp));
    }
    
    logTransaction(m_transaction_log, in_txn_id, in_file_name);
    
    START_TRANSACTION_TIMER();
}

void ServerBackend::initializeFunctions()
{
    m_txn_timer_function = [this](const TxnId in_txn_id, Timestamp in_latest_timestamp, const FileName in_file_name)
    {
        while (1)
        {
            std::this_thread::sleep_until(in_latest_timestamp + std::chrono::seconds(Constants::transaction_timeout_seconds));
            
            lock_guard<mutex> member_grd(m_member_mtx);
            
            auto txn_it = m_txn_id_to_transaction_attributes.find(in_txn_id);
            
            if (end(m_txn_id_to_transaction_attributes) != txn_it)
            {
                auto& [txn_id, txn_tuple] = *txn_it;
                
                auto& [sp_txn_mtx, sp_file_attributes, buffer, max_seq_num, curr_timestamp] = txn_tuple;
                
                if (NOW >= (curr_timestamp + std::chrono::seconds(Constants::transaction_timeout_seconds))) // transaction timeout
                {
                    removeTransaction(txn_it);
                    
                    logTransaction(m_timeout_log, in_txn_id, in_file_name);
                    
                    return;
                }
                else
                {
                    in_latest_timestamp = curr_timestamp;
                }
            }
            else
            {
                return;
            }
        }
    };
    
    CommandFunction READ = COMMAND_FUNCTION_PARAMS
    {
        // Note: The READ command opts for availability over consistency in that if the read file
        //       is currently being written, only the number of bytes in the file at the time the
        //       read function is called will be returned to the client.
        
        auto& [command, txn_id, seq_num, content_len, file_name] = in_client_request_tuple;
        
        try // TODO: handle case where file is too large to load into memory
        {
            File file(m_directory + file_name, O_RDONLY);
            
            SET_READ_AND_RETURN(file.read());
        }
        catch (Exception::ErrorOpeningFile)
        {
            SET_ERROR_AND_RETURN(Errors::ErrorOpeningFile);
        }
        catch (Exception::ErrorReadingFromFile)
        {
            SET_ERROR_AND_RETURN(Errors::ErrorReadingFile);
        }
    };
    
    CommandFunction NEW_TXN = COMMAND_FUNCTION_PARAMS
    {
        auto& [command, txn_id, seq_num, content_len, file_name_const] = in_client_request_tuple;
        
        if (0 == seq_num)
        {
            TxnId candidate_id;
            
            lock_guard<mutex> member_grd(m_member_mtx);
            
            while (m_txn_id_to_transaction_attributes.count(candidate_id = rand() % INT32_MAX));
            
            try
            {
                auto& file_name = const_cast<FileName&>(file_name_const);
                
                addNewTransaction(candidate_id, move(file_name));
            }
            catch (Exception::ErrorAddingFileAttributes)
            {
                SET_ERROR_AND_RETURN(Errors::ErrorCreatingTransaction);
            }
            
            SET_NEW_TXN_AND_RETURN(candidate_id);
        }
        else
        {
            SET_ERROR_AND_RETURN(Errors::InvalidSequenceNumber);
        }
    };
    
    CommandFunction WRITE = COMMAND_FUNCTION_PARAMS
    {
        unique_lock<mutex> member_lck(m_member_mtx);
        
        auto& [command, txn_id, seq_num, content_len, data] = in_client_request_tuple;
        
        RETURN_ERROR_IF_COMMITTED_OR_INVALID_ID();
        
        auto& txn_tuple = m_txn_id_to_transaction_attributes[txn_id];
        
        auto& [sp_txn_mtx, sp_file_attributes, buffers, max_seq_num, curr_timestamp] = txn_tuple;
        
        // make a copy of the shared_ptr to ensure the mutex remains allocated for subsequent
        // acquire
        auto sp_txn_mtx_cpy = sp_txn_mtx;
        
        member_lck.unlock();
        
        lock_guard<mutex> transaction_grd(*sp_txn_mtx_cpy);
        
        member_lck.lock();
        
        // Note: If true another process/thread operating on this transaction has since
        //       committed/aborted. Return error since concurrently attempting to write to and
        //       commit/abort the same transaction is an error on the client-side.
        RETURN_ERROR_IF_COMMITTED_OR_ABORTED();
        
        member_lck.unlock();
        
        updateTransactionTimestamp(curr_timestamp);
        
        if (buffers.count(seq_num))
        {
            SET_ERROR_AND_RETURN(Errors::RepeatedSequenceNumber);
        }
        else
        {
            // Note: max_seq_num = max(max_seq_num, seq_num); has no effect for optimized builds
            if (seq_num > max_seq_num)
            {
                max_seq_num = seq_num;
            }
            
            buffers[seq_num] = move(data);
            
            SET_ACK_AND_RETURN();
        }
    };
    
    CommandFunction COMMIT = COMMAND_FUNCTION_PARAMS
    {
        unique_lock<mutex> member_lck(m_member_mtx);
        
        const auto& [command, txn_id, seq_num, content_len, data] = in_client_request_tuple;
        
        // Note: Return ACK here if committed as the client may be retransmitting the COMMIT due
        //       to lost ACK from initial COMMIT.
        RETURN_ACK_IF_COMMITTED_OR_ERROR_IF_INVALID_ID();
        
        auto& txn_tuple = m_txn_id_to_transaction_attributes[txn_id];
        
        auto& [sp_txn_mtx, sp_file_attributes, buffers, max_seq_num, curr_timestamp] = txn_tuple;
        
        // make a copy of the shared_ptr to ensure the mutex remains allocated for subsequent
        // acquire
        auto sp_txn_mtx_cpy = sp_txn_mtx;
        
        member_lck.unlock();
        
        lock_guard<mutex> transaction_grd(*sp_txn_mtx_cpy);
        
        member_lck.lock();
        
        // Note: If true another process/thread operating on this transaction has since
        //       committed/aborted. Return error since concurrently attempting to commit and
        //       commit/abort the same transaction is an error on the client-side.
        RETURN_ERROR_IF_COMMITTED_OR_ABORTED();
        
        member_lck.unlock();
        
        updateTransactionTimestamp(curr_timestamp);
        
        if (seq_num < max_seq_num)
        {
            SET_ERROR_AND_RETURN(Errors::CommitWithInvalidSequenceNumber);
        }
        else
        {
            max_seq_num = seq_num;
        }
        
        const auto& file_name = sp_file_attributes->m_file_name;
        
        auto& file_size = sp_file_attributes->m_file_size;
        
        auto& file_mtx = sp_file_attributes->m_file_mtx;
        
        for (SeqNum seq_num = Constants::initial_seq_num + 1; seq_num <= max_seq_num; ++seq_num) // verify all seq nums received
        {
            if (0 == buffers.count(seq_num))
            {
                SET_ASK_RESEND_AND_RETURN(seq_num);
            }
        }
        
        try
        {
            lock_guard<mutex> file_grd(file_mtx);
            
            File file(m_directory + file_name, O_CREAT | O_WRONLY | O_APPEND);
            
            for (SeqNum seq_num = Constants::initial_seq_num + 1; seq_num <= max_seq_num; ++seq_num)
            {
                file.write(buffers.at(seq_num));
            }
            
            m_commits.insert(txn_id);
            
            logTransaction(m_commit_log, txn_id, file_name);
            
            file_size = file.getFileSize();
        }
        catch (Exception::ErrorOpeningFile)
        {
            SET_ERROR_AND_RETURN(Errors::ErrorOpeningFile);
        }
        catch (Exception::ErrorWritingToFile)
        {
            truncate(file_name.c_str(), file_size);
            
            SET_ERROR_AND_RETURN(Errors::ErrorWritingFile);
        }
        
        // about to access m_txn_id_to_transaction_attributes again so must lock critical section
        // must find id in m_txn_id_to_transaction_attributes again as an earlier iterator to
        // this entry may have since been invalidated
        member_lck.lock();
        
        removeTransaction(m_txn_id_to_transaction_attributes.find(txn_id));
        
        SET_ACK_AND_RETURN();
    };
    
    CommandFunction ABORT = COMMAND_FUNCTION_PARAMS
    {
        unique_lock<mutex> member_lck(m_member_mtx);
        
        const auto& [command, txn_id, seq_num, content_len, data] = in_client_request_tuple;
        
        RETURN_ERROR_IF_COMMITTED_OR_INVALID_ID();
        
        auto& txn_tuple = m_txn_id_to_transaction_attributes[txn_id];
        
        auto& [sp_txn_mtx, sp_file_attributes, buffers, max_seq_num, curr_timestamp] = txn_tuple;
        
        // make a copy of the shared_ptr to ensure the mutex remains allocated for subsequent
        // acquire
        auto sp_txn_mtx_cpy = sp_txn_mtx;
        
        member_lck.unlock();
        
        unique_lock<mutex> transaction_lck(*sp_txn_mtx_cpy);
        
        member_lck.lock();
        
        // Note: If true another process/thread operating on this transaction has since
        //       committed/aborted. Return error since concurrently attempting to abort and
        //       commit/abort the same transaction is an error on the client-side.
        RETURN_ERROR_IF_COMMITTED_OR_ABORTED();
        
        out_transaction_in_progress = false;
        
        // Note: This is safe because if a WRITEr or COMMITter acquires this released lock
        //       immediately they still have to acquire the member_lck as the very next step
        //       (i.e. so they can check if the transaction is valid). However, because ABORT has
        //       the member_lck here they can't acquire the member_lck until after the
        //       transaction has been aborted and will therefore return an error to the client
        //       upon acquiring the member_lck (i.e. so there is no window for the WRITEr or
        //       COMMITer to modify or access the aborted transaction).
        transaction_lck.unlock();
        
        const auto& file_name = sp_file_attributes->m_file_name;
        
        logTransaction(m_abort_log, txn_id, file_name);
        
        removeTransaction(m_txn_id_to_transaction_attributes.find(txn_id));
        
        SET_ACK_AND_RETURN();
    };
    
    m_command_to_function = {{Constants::read_cmd, READ},{Constants::new_txn_cmd, NEW_TXN},{Constants::write_cmd, WRITE},{Constants::commit_cmd, COMMIT},{Constants::abort_cmd, ABORT}};
}

void ServerBackend::initializeTransactions()
{
    // Files and curr_transactions must remain as separate data structures as not all files
    // operated on during the period the system was last up will be currently associated
    // with a transaction.
    
    FileNameFileSizeMap file_names_to_file_sizes;
    
    TxnIdFileNameMap txn_ids_to_file_names;
    
    loadFilesAndTransactions(file_names_to_file_sizes, txn_ids_to_file_names);
    
    // Ensure each file is no greater in size than its corresponding maximum size in each of
    // the logs. If it is greater in size, the additional data is from a transaction that was
    // commiting data to disk at the time of the server crash, but had yet to write to the
    // log file to make the transaction commit official. Therefore in this case, rollback
    // the writes to the file.
    truncateFiles(file_names_to_file_sizes);
    
    for (auto& [txn_id, file_name] : txn_ids_to_file_names) // restart transactions
    {
        addNewTransaction(txn_id, move(file_name));
    }
}

void ServerBackend::initializeFunctionsAndTransactions()
{
    if (m_initialize)
    {
        lock_guard<mutex> member_grd(m_member_mtx);
        
        if (m_initialize)
        {
            // Note: initializeFunctions must be called before initializeTransactions because
            //       initializeTransactions depends on m_txn_timer_function.
            initializeFunctions();
            
            initializeTransactions();
            
            m_initialize.store(false);
        }
    }
}

void ServerBackend::loadFilesAndTransactions(FileNameFileSizeMap& out_file_names_to_file_sizes, TxnIdFileNameMap& out_txn_ids_to_file_names)
{
    // m_transaction_log must be first as subsequent logs will be used to prune
    // out_txn_ids_to_file_names
    const auto logs = {m_transaction_log, m_timeout_log, m_commit_log, m_abort_log};
    
    for (const auto& log_file : logs)
    {
        ifstream log(log_file, ifstream::in);
        
        if (log)
        {
            stringstream ss;
            
            ss << log.rdbuf();
            
            log.close();
            
            string entry;
            
            TxnId txn_id;
            
            FileName file_name;
            
            FileSize file_size;
            
            while (ss >> txn_id && ss >> file_name && ss >> file_size)
            {
                auto fntfs_it = out_file_names_to_file_sizes.find(file_name);
                
                if (end(out_file_names_to_file_sizes) == fntfs_it)
                {
                    out_file_names_to_file_sizes[file_name] = file_size;
                }
                // Note: fntfs_it->second = max(fntfs_it->second, file_size); has no effect for optimized
                //       builds
                else if (file_size > fntfs_it->second)
                {
                    fntfs_it->second = file_size;
                }
                
                auto titfn_it = out_txn_ids_to_file_names.find(txn_id);
                
                if (end(out_txn_ids_to_file_names) == titfn_it)
                {
                    out_txn_ids_to_file_names[txn_id] = move(file_name);
                }
                else // this transaction timed out, committed, or aborted
                {
                    out_txn_ids_to_file_names.erase(titfn_it);
                }
            }
            if (-1 == remove(log_file.c_str())) // erase hidden log
            {
                perror("Error deleting file");
                
                exit(EXIT_FAILURE);
            }
        }
    }
}

void ServerBackend::logTransaction(const FileName& in_log_name, const TxnId in_txn_id, const FileName& in_file_name)
{
    try
    {
        File log(m_directory + in_log_name, O_CREAT | O_WRONLY | O_APPEND);
        
        log.write(to_string(in_txn_id));
        
        log.write(" ");
        
        log.write(in_file_name);
        
        log.write(" ");
        
        log.write(to_string(File::getFileSize(in_file_name)));
        
        log.write("\n");
    }
    catch (Exception::ErrorOpeningFile)
    {
#ifdef DEBUG
        if (std::unique_lock<std::mutex> global_lck(g_mtx); global_lck.owns_lock())
        {
            perror("Error opening log file");
        }
#endif
    }
    catch(Exception::ErrorWritingToFile)
    {
#ifdef DEBUG
        if (std::unique_lock<std::mutex> global_lck(g_mtx); global_lck.owns_lock())
        {
            perror("Error writing to log file");
        }
#endif
    }
}

void ServerBackend::processCommand(const RequestTuple& in_client_request_tuple, string& out_server_response_str, bool& out_transaction_in_progress)
{
    const auto& [command, txn_id, seq_num, content_len, data] = in_client_request_tuple;
    
    if (m_command_to_function.count(command))
    {
        m_command_to_function[command](in_client_request_tuple, out_server_response_str, out_transaction_in_progress);
    }
    else // command not found
    {
        SET_ERROR_AND_RETURN(Errors::InvalidCommand);
    }
}

void ServerBackend::removeTransaction(TransactionAttributesMapIterator in_txn_it)
{
    if (end(m_txn_id_to_transaction_attributes) != in_txn_it)
    {
        m_txn_id_to_transaction_attributes.erase(in_txn_it);
    }
}

void ServerBackend::truncateFiles(const FileNameFileSizeMap& in_file_names_to_file_sizes)
{
    for (const auto& [file_name, file_size] : in_file_names_to_file_sizes)
    {
        if (File::fileExists(file_name))
        {
            if (0 < file_size)
            {
                if (-1 == truncate(file_name.c_str(), file_size))
                {
                    perror("Error truncating file");
                    
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                if (-1 == remove(file_name.c_str()))
                {
                    perror("Error deleting file");
                    
                    exit(EXIT_FAILURE);
                }
            }
        }
    }
}

void ServerBackend::updateTransactionTimestamp(Timestamp& out_timestamp)
{
    out_timestamp = NOW;
}

// ↑                                                                                            ↑ //
// Private Member Functions                                                                       //
////////////////////////////////////////////////////////////////////////////////////////////////////
