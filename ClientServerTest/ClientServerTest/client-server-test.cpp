//
//  client-server-test.cpp
//  ClientServerTest
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// The Client Server Test Suite uses Google Test and has been verified to work with version       //
// 1.8.x: https://github.com/google/googletest/tree/v1.8.x                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: add test where request only matches the first 2/3s of the regex

// TODO: add test for ensuring file not seen on file system until transaction commits (verify this with a read request)

// TODO: add test for reading from a very large file

// TODO: add test for multiple concurrent transactions from single client

// TODO: add test where requests have invalid content length values

#include <algorithm>
#include <array>
#include <future>
#include <numeric>
#include <random>
#include <vector>

#include "client.h"
#include "constants.h"
#include "errors.h"
#include "gtest/gtest.h"

#define CLI_ARGS g_server_ipv4_addr, stoi(g_server_port)

using std::chrono::milliseconds;

using std::chrono::seconds;

using std::chrono::system_clock;

using std::default_random_engine;

using std::get;

using std::iota;

using std::shuffle;

using std::stoi;

using std::string;

using std::this_thread::sleep_for;

using std::thread;

using std::vector;

static constexpr auto to_string = [](auto t) constexpr -> decltype(auto) { return std::to_string(t);};

using namespace EmersonClientServerFileSystem;

extern std::string g_server_ipv4_addr;

extern std::string g_server_port;

extern std::string g_server_directory;

namespace ResponseFields
{
    enum ResponseFormat{Command, TxnId, SeqNum, ErrorCode, ContentLen, Data};
}

void eraseFile(const string& in_file_name)
{
    if (!g_server_directory.empty())
    {
        string file_path = g_server_directory + in_file_name;
        
        if (-1 == remove(file_path.c_str()))
        {
            GTEST_FATAL_FAILURE_("Server failed to create file");
        }
    }
}

TEST(ClientOmission, OmittedSequenceNumber)
{
    Client client(CLI_ARGS);
    
    string expected;
    
    string file_name = "File" + to_string(rand()) + ".txt";
    
    int txn_id = Constants::default_txn_id;
    
    auto server_response_tuple = client.sendRequestGetResponse(Constants::new_txn_cmd, txn_id, Constants::initial_seq_num, file_name);
    
    txn_id = get<ResponseFields::TxnId>(server_response_tuple);
    
    EXPECT_NE(txn_id, Constants::default_txn_id);
    
    const int num_requests = 50;
    
    const int missing_seq_num = 1 + (rand() % num_requests);
    
    for (int seq_num = Constants::initial_seq_num + 1; seq_num <= num_requests; ++seq_num)
    {
        string data = to_string(seq_num);
        
        if (!(missing_seq_num == seq_num))
        {
            client.sendRequestGetResponse(Constants::write_cmd, txn_id, seq_num, data);
        }
        
        expected += data;
    }
    
    server_response_tuple = client.sendRequestGetResponse(Constants::commit_cmd, txn_id, num_requests);
    
    EXPECT_STREQ(Constants::ask_resend_cmd, get<ResponseFields::Command>(server_response_tuple).c_str());
    
    EXPECT_EQ(missing_seq_num, get<ResponseFields::SeqNum>(server_response_tuple));
    
    client.sendRequestGetResponse(Constants::write_cmd, txn_id, missing_seq_num, to_string(missing_seq_num));
    
    server_response_tuple = client.sendRequestGetResponse(Constants::commit_cmd, txn_id, num_requests);
    
    EXPECT_STREQ(Constants::ack_cmd, get<ResponseFields::Command>(server_response_tuple).c_str());
    
    server_response_tuple = client.sendRequestGetResponse(Constants::read_cmd, Constants::default_txn_id, Constants::initial_seq_num, file_name);
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), expected.c_str());
    
    eraseFile(file_name);
}

TEST(ClientByzantine, InvalidRequestFormat)
{
    Client client(CLI_ARGS);
    
    // Note: this will work with a request shorter than the request header length, but will result in a longer wait time
    //       as server will try to read request header length until timeout
    string request = "dsjfhaskdjfhasdfgsdkjfahsdgfkajshdgfasjdfgjkasdsjfhaskdjfhasdfgsdkjfahsdgfkajshdgfasjdfgjkasdsjfhaskdjfhasdfgsdkjfahsdgfkajshdgfasjdfgjkas";
    
    ASSERT_FALSE(Constants::wire_protocol.isValidRequestFormat(request.c_str()));
    
    auto server_response_tuple = client.sendRawRequestGetResponse(request.c_str(), request.length());
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), Errors::getErrorMessage(Errors::InvalidMessageFormat).c_str());
}

TEST(ClientByzantine, InvalidCommand)
{
    Client client(CLI_ARGS);
    
    string data = "Here is my data that goes into file";
    
    string invalid_command = "WRRITE";
    
    ASSERT_NE(invalid_command, Constants::abort_cmd);
    
    ASSERT_NE(invalid_command, Constants::commit_cmd);
    
    ASSERT_NE(invalid_command, Constants::new_txn_cmd);
    
    ASSERT_NE(invalid_command, Constants::read_cmd);
    
    ASSERT_NE(invalid_command, Constants::write_cmd);
    
    auto server_response_tuple = client.sendRequestGetResponse(invalid_command.c_str(), Constants::default_txn_id , Constants::initial_seq_num, data);
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), Errors::getErrorMessage(Errors::InvalidCommand).c_str());
}

TEST(ClientByzantine, InvalidTransactionId)
{
    Client client(CLI_ARGS);
    
    string data = "Here is my data that goes into file";
    
    auto server_response_tuple = client.sendRequestGetResponse(Constants::write_cmd, Constants::default_txn_id, Constants::initial_seq_num, data);
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), Errors::getErrorMessage(Errors::InvalidTransactionId).c_str());
}

TEST(ClientByzantine, InvalidInitialSequenceNumber)
{
    Client client(CLI_ARGS);
    
    int seq_num = 1;
    
    string file_name = "File" + to_string(rand()) + ".txt";
    
    ASSERT_NE(seq_num, Constants::initial_seq_num);
    
    auto server_response_tuple = client.sendRequestGetResponse(Constants::new_txn_cmd, Constants::default_txn_id, seq_num, file_name);
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), Errors::getErrorMessage(Errors::InvalidSequenceNumber).c_str());
}

TEST(ClientByzantine, RepeatedSequenceNumber)
{
    Client client(CLI_ARGS);
    
    int txn_id = Constants::default_txn_id;
    
    int seq_num = Constants::initial_seq_num;
    
    string data = "Here is my data that goes into file";
    
    string file_name = "File" + to_string(rand()) + ".txt";
    
    auto server_response_tuple = client.sendRequestGetResponse(Constants::new_txn_cmd, txn_id, seq_num++, file_name);
    
    txn_id = get<ResponseFields::TxnId>(server_response_tuple);
    
    EXPECT_NE(txn_id, Constants::default_txn_id);
    
    client.sendRequestGetResponse(Constants::write_cmd, txn_id, seq_num, data);
    
    server_response_tuple = client.sendRequestGetResponse(Constants::write_cmd, txn_id, seq_num, data);
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), Errors::getErrorMessage(Errors::RepeatedSequenceNumber).c_str());
}

TEST(ClientByzantine, CommitWithInvalidSequenceNumber)
{
    Client client(CLI_ARGS);
    
    int txn_id = Constants::default_txn_id;
    
    int seq_num = Constants::initial_seq_num;
    
    string data = "Here is my data that goes into file";
    
    string file_name = "File" + to_string(rand()) + ".txt";
    
    auto server_response_tuple = client.sendRequestGetResponse(Constants::new_txn_cmd, txn_id, seq_num++, file_name);
    
    txn_id = get<ResponseFields::TxnId>(server_response_tuple);
    
    EXPECT_NE(txn_id, Constants::default_txn_id);
    
    client.sendRequestGetResponse(Constants::write_cmd, txn_id, seq_num + 1, data);
    
    server_response_tuple = client.sendRequestGetResponse(Constants::commit_cmd, txn_id, seq_num);
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), Errors::getErrorMessage(Errors::CommitWithInvalidSequenceNumber).c_str());
}

TEST(ClientByzantine, ReadNonexistentFile)
{
    Client client(CLI_ARGS);
    
    auto server_response_tuple = client.sendRequestGetResponse(Constants::read_cmd, Constants::default_txn_id, Constants::initial_seq_num, "NonexistentFile.txt");
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), Errors::getErrorMessage(Errors::ErrorOpeningFile).c_str());
}

TEST(ClientByzantine, SendPacketsOutOfOrder)
{
    Client client(CLI_ARGS);
    
    string expected;
    
    int txn_id = Constants::default_txn_id;
    
    string file_name = "SendPacketsOutOfOrder" + to_string(rand()) + ".txt";
    
    auto server_response_tuple = client.sendRequestGetResponse(Constants::new_txn_cmd, txn_id, Constants::initial_seq_num, file_name);
    
    txn_id = get<ResponseFields::TxnId>(server_response_tuple);
    
    EXPECT_NE(txn_id, Constants::default_txn_id);
    
    const int num_requests = 50;
    
    vector<int> seq_nums(num_requests);
    
    iota(begin(seq_nums), end(seq_nums), 1);
    
    for (auto seq_num : seq_nums)
    {
        expected += to_string(seq_num);
    }
    
    unsigned seed = static_cast<unsigned>(system_clock::now().time_since_epoch().count());
    
    shuffle(begin(seq_nums), end(seq_nums), default_random_engine(seed));
    
    for (int i = 0; i < num_requests; ++i)
    {
        client.sendRequestGetResponse(Constants::write_cmd, txn_id, seq_nums[i], to_string(seq_nums[i]));
    }
    
    server_response_tuple = client.sendRequestGetResponse(Constants::commit_cmd, txn_id, num_requests);
    
    server_response_tuple = client.sendRequestGetResponse(Constants::read_cmd, Constants::default_txn_id, Constants::initial_seq_num, file_name);
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), expected.c_str());
    
    eraseFile(file_name);
}

TEST(ClientByzantine, WriteToCommittedTransaction)
{
    Client client1(CLI_ARGS);
    // need second client because when first client causes server to return error, server will
    // close connection (i.e. sending any more messages from client1 will fail)
    Client client2(CLI_ARGS);
    
    string data = "Here is my data that goes into file";
    
    int txn_id = Constants::default_txn_id;
    
    int seq_num = Constants::initial_seq_num;
    
    string file_name = "File" + to_string(rand()) + ".txt";
    
    auto server_response_tuple = client1.sendRequestGetResponse(Constants::new_txn_cmd, txn_id, seq_num++, file_name);
    
    txn_id = get<ResponseFields::TxnId>(server_response_tuple);
    
    EXPECT_NE(txn_id, Constants::default_txn_id);
    
    client1.sendRequestGetResponse(Constants::write_cmd, txn_id, seq_num, data);
    
    client1.sendRequestGetResponse(Constants::commit_cmd, txn_id, seq_num);
    
    server_response_tuple = client1.sendRequestGetResponse(Constants::write_cmd, txn_id, seq_num++, data);
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), Errors::getErrorMessage(Errors::InvalidOperation).c_str());
    
    server_response_tuple = client2.sendRequestGetResponse(Constants::write_cmd, txn_id, seq_num, data);
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), Errors::getErrorMessage(Errors::InvalidOperation).c_str());
    
    eraseFile(file_name);
}

TEST(ClientFailstop, TransactionTimeout)
{
    Client client1(CLI_ARGS);
    
    string data = "Here is my data that goes into file";
    
    int txn_id = Constants::default_txn_id;
    
    int seq_num = Constants::initial_seq_num;
    
    string file_name = "File" + to_string(rand()) + ".txt";
    
    auto server_response_tuple = client1.sendRequestGetResponse(Constants::new_txn_cmd, txn_id, seq_num++, file_name);
    
    txn_id = get<ResponseFields::TxnId>(server_response_tuple);
    
    EXPECT_NE(txn_id, Constants::default_txn_id);
    
    sleep_for(seconds(Constants::transaction_timeout_seconds));
    
    Client client2(CLI_ARGS);
    
    server_response_tuple = client2.sendRequestGetResponse(Constants::write_cmd, txn_id, seq_num, data);
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), Errors::getErrorMessage(Errors::InvalidTransactionId).c_str());
}

TEST(NetworkFailure, LostAck)
{
    Client client(CLI_ARGS);
    
    string data = "Here is my data that goes into file";
    
    int txn_id = Constants::default_txn_id;
    
    int seq_num = Constants::initial_seq_num;
    
    string file_name = "File" + to_string(rand()) + ".txt";
    
    auto server_response_tuple = client.sendRequestGetResponse(Constants::new_txn_cmd, txn_id, seq_num++, file_name);
    
    txn_id = get<ResponseFields::TxnId>(server_response_tuple);
    
    EXPECT_NE(txn_id, Constants::default_txn_id);
    
    client.sendRequestGetResponse(Constants::write_cmd, txn_id, seq_num, data);
    
    client.sendRequestGetResponse(Constants::commit_cmd, txn_id, seq_num);
    
    server_response_tuple = client.sendRequestGetResponse(Constants::commit_cmd, txn_id, seq_num);
    
    EXPECT_STREQ(Constants::ack_cmd, get<ResponseFields::Command>(server_response_tuple).c_str());
    
    eraseFile(file_name);
}

TEST(Client, NewTransaction)
{
    Client client(CLI_ARGS);
    
    string file_name = "File" + to_string(rand()) + ".txt";
    
    auto [command, txn_id, seq_num, error_code, content_len, data] = client.sendRequestGetResponse(Constants::new_txn_cmd, Constants::default_txn_id, Constants::initial_seq_num, file_name);
    
    EXPECT_STREQ(Constants::ack_cmd, command.c_str());
    
    EXPECT_GT(txn_id, Constants::default_txn_id);
    
    EXPECT_EQ(0u, seq_num);
    
    EXPECT_EQ(0u, error_code);
    
    EXPECT_EQ(0u, content_len);
    
    EXPECT_STREQ("", data.c_str());
}

TEST(Client, PipelinedRequests)
{
    Client client(CLI_ARGS);
    
    string file_name = "File" + to_string(rand()) + ".txt";
    
    string expected;
    
    int txn_id = Constants::default_txn_id;
    
    int seq_num = Constants::initial_seq_num;
    
    auto server_response_tuple = client.sendRequestGetResponse(Constants::new_txn_cmd, txn_id, seq_num++, file_name);
    
    txn_id = get<ResponseFields::TxnId>(server_response_tuple);
    
    EXPECT_NE(txn_id, Constants::default_txn_id);
    
    const int num_requests = 100;
    
    for (int i = 0; i < num_requests; ++i)
    {
        int numbers_per_request = 15;
        
        string data;
        
        for (int j = 0; j < numbers_per_request; ++j)
        {
            data += to_string(rand());
        }
        
        client.sendRequest(Constants::write_cmd, txn_id, seq_num++, data);
        
        expected += data;
    }
    
    for (int i = 0; i < num_requests; ++i)
    {
        client.getResponse();
    }
    
    client.sendRequestGetResponse(Constants::commit_cmd, txn_id, seq_num - 1);
    
    server_response_tuple = client.sendRequestGetResponse(Constants::read_cmd, Constants::default_txn_id, Constants::initial_seq_num, file_name);
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), expected.c_str());
    
    eraseFile(file_name);
}

TEST(Client, ReceiveAckOnCommit)
{
    Client client(CLI_ARGS);
    
    int txn_id = Constants::default_txn_id;
    
    int seq_num = Constants::initial_seq_num;
    
    string file_name = "File" + to_string(rand()) + ".txt";
    
    auto server_response_tuple = client.sendRequestGetResponse(Constants::new_txn_cmd, txn_id, seq_num++, file_name);
    
    txn_id = get<ResponseFields::TxnId>(server_response_tuple);
    
    EXPECT_NE(txn_id, Constants::default_txn_id);
    
    client.sendRequestGetResponse(Constants::write_cmd, txn_id, seq_num, "Here is my data that goes into file");
    
    server_response_tuple = client.sendRequestGetResponse(Constants::commit_cmd, txn_id, seq_num);
    
    EXPECT_STREQ(Constants::ack_cmd, get<ResponseFields::Command>(server_response_tuple).c_str());
    
    eraseFile(file_name);
}

TEST(Client, MultipleClientInteractionSameTransactionSequential)
{
    Client client(CLI_ARGS);
    
    string expected;
    
    int txn_id = Constants::default_txn_id;
    
    string file_name = "File" + to_string(rand()) + ".txt";
    
    auto server_response_tuple = client.sendRequestGetResponse(Constants::new_txn_cmd, txn_id, Constants::initial_seq_num, file_name);
    
    txn_id = get<ResponseFields::TxnId>(server_response_tuple);
    
    EXPECT_NE(txn_id, Constants::default_txn_id);
    
    const int num_requests = 50;
    
    for (int seq_num = num_requests; seq_num > Constants::initial_seq_num; --seq_num)
    {
        Client client(CLI_ARGS);
        
        string data = to_string(rand());
        
        client.sendRequestGetResponse(Constants::write_cmd, txn_id, seq_num, data);
        
        expected.insert(0, data);
    }
    
    server_response_tuple = client.sendRequestGetResponse(Constants::commit_cmd, txn_id, num_requests);
    
    EXPECT_STREQ(Constants::ack_cmd, get<ResponseFields::Command>(server_response_tuple).c_str());
    
    server_response_tuple = client.sendRequestGetResponse(Constants::read_cmd, Constants::default_txn_id, Constants::initial_seq_num, file_name);
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), expected.c_str());
    
    eraseFile(file_name);
}

TEST(Client, MultipleClientInteractionSameTransactionParallel)
{
    Client client(CLI_ARGS);
    
    string expected;
    
    int txn_id = Constants::default_txn_id;
    
    string file_name = "File" + to_string(rand()) + ".txt";
    
    auto server_response_tuple = client.sendRequestGetResponse(Constants::new_txn_cmd, txn_id, Constants::initial_seq_num, file_name);
    
    txn_id = get<ResponseFields::TxnId>(server_response_tuple);
    
    EXPECT_NE(txn_id, Constants::default_txn_id);
    
    const int num_clients = 50;
    
    thread threads[num_clients + 1];
    
    for (int seq_num = num_clients; seq_num > Constants::initial_seq_num; --seq_num)
    {
        string data = to_string(rand());
        
        threads[seq_num] = thread([=]()
                                  {
                                      Client client(CLI_ARGS);
                                      
                                      sleep_for(milliseconds(seq_num));
                                      
                                      client.sendRequestGetResponse(Constants::write_cmd, txn_id, seq_num, data);
                                  });
        
        expected.insert(0, data);
    }
    
    for (int cid = num_clients; cid > Constants::initial_seq_num; --cid)
    {
        threads[cid].join();
    }
    
    server_response_tuple = client.sendRequestGetResponse(Constants::commit_cmd, txn_id, num_clients);
    
    EXPECT_STREQ(Constants::ack_cmd, get<ResponseFields::Command>(server_response_tuple).c_str());
    
    server_response_tuple = client.sendRequestGetResponse(Constants::read_cmd, Constants::default_txn_id, Constants::initial_seq_num, file_name);
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), expected.c_str());
    
    eraseFile(file_name);
}

TEST(Client, AbortTransaction)
{
    Client client1(CLI_ARGS);
    
    Client client2(CLI_ARGS); // need second client because server will close connection when first client sends abort
    
    string data = "Here is my data that goes into file";
    
    int txn_id = Constants::default_txn_id;
    
    string file_name = "File" + to_string(rand()) + ".txt";
    
    auto server_response_tuple = client1.sendRequestGetResponse(Constants::new_txn_cmd, txn_id, Constants::initial_seq_num, file_name);
    
    txn_id = get<ResponseFields::TxnId>(server_response_tuple);
    
    EXPECT_NE(txn_id, Constants::default_txn_id);
    
    const int num_requests = 50;
    
    for (int seq_num = Constants::initial_seq_num + 1; seq_num < num_requests; ++seq_num)
    {
        client1.sendRequestGetResponse(Constants::write_cmd, txn_id, seq_num, data);
    }
    
    server_response_tuple = client1.sendRequestGetResponse(Constants::abort_cmd, txn_id, num_requests);
    
    EXPECT_STREQ(Constants::ack_cmd, get<ResponseFields::Command>(server_response_tuple).c_str());
    
    server_response_tuple = client2.sendRequestGetResponse(Constants::commit_cmd, txn_id, num_requests);
    
    EXPECT_STREQ(get<ResponseFields::Data>(server_response_tuple).c_str(), Errors::getErrorMessage(Errors::InvalidTransactionId).c_str());
}
