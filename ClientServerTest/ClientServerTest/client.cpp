//
//  client.cpp
//  ClientServerTest
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//

#include <arpa/inet.h>
#include <unistd.h>

#include "client.h"
#include "constants.h"
#include "gtest/gtest.h"
#include "read-write-helper.h"

using namespace EmersonClientServerFileSystem;

////////////////////////////////////////////////////////////////////////////////////////////////////
// Public Member Functions                                                                        //
// ↓                                                                                            ↓ //

Client::Client(string in_serv_ipv4_addr, int in_serv_portno) : m_serv_ipv4_addr(move(in_serv_ipv4_addr)), m_serv_portno(in_serv_portno)
{
    initializeSocket();
    
    connectToServer();
}

Client::~Client()
{
    close(m_sockfd);
}

Client::ResponseTuple Client::getResponse()
{
    ResponseTuple server_response_tuple;
    
    char response_header[Constants::response_header_len + 1]; // add 1 for null terminator
    
    bzero(response_header, Constants::response_header_len + 1);
    
    if (ReadWriteHelper::readFileDescriptor(m_sockfd, response_header, Constants::response_header_len) < 0)
    {
        perror("Error reading from socket");
        
        exit(EXIT_FAILURE);
    }
    else
    {
        EXPECT_TRUE(Constants::wire_protocol.isValidResponseFormat(response_header));
        
        Constants::wire_protocol.extractHeaderFields(response_header, server_response_tuple);
        
        auto& [command, txn_id, seq_num, error_code, content_len, data] = server_response_tuple;
        
        char response_payload[content_len + 1]; // add 1 for null terminator
        
        bzero(response_payload, content_len + 1);
        
        if (ReadWriteHelper::readFileDescriptor(m_sockfd, response_payload, content_len) < 0)
        {
            perror("Error reading from socket");
            
            exit(EXIT_FAILURE);
        }
        else
        {
            data = response_payload;
        }
    }
    
    return server_response_tuple;
}

void Client::printResponse(const ResponseTuple& in_server_response_tuple)
{
    const auto& [command, id, seq_num, error_code, content_len, data] = in_server_response_tuple;
    
    std::cout << command << " " << id << " " << seq_num << " " << error_code << " " << content_len << " " << data << std::endl;
}

void Client::sendRequest(const string& in_command, int in_txn_id, int in_seq_num, const string& in_data)
{
    sendRequest(in_command, in_txn_id, in_seq_num, in_data, false);
}

Client::ResponseTuple Client::sendRawRequestGetResponse(const char * in_raw_request, size_t in_raw_request_len)
{
    assert(!(nullptr == in_raw_request));
    
    assert(strlen(in_raw_request) == in_raw_request_len);
    
    if (ReadWriteHelper::writeFileDescriptor(m_sockfd, in_raw_request, in_raw_request_len) < 0)
    {
        perror("Error writing to socket");
        
        exit(EXIT_FAILURE);
    }
    
    return getResponse();
}

Client::ResponseTuple Client::sendRequestGetResponse(const string& in_command, int in_txn_id, int in_seq_num, const string& in_data)
{
    return sendRequest(in_command, in_txn_id, in_seq_num, in_data, true);
}

// ↑                                                                                            ↑ //
// Public Member Functions                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
// Private Member Functions                                                                       //
// ↓                                                                                            ↓ //

void Client::connectToServer()
{
    if (connect(m_sockfd,(struct sockaddr *) &m_serv_addr, sizeof(m_serv_addr)) < 0)
    {
        perror("Error connecting");
        
        exit(EXIT_FAILURE);
    }
}

void Client::initializeSocket()
{
    m_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (m_sockfd < 0)
    {
        perror("Error opening socket");
        
        exit(EXIT_FAILURE);
    }
    
    m_serv_len = sizeof(m_serv_addr);
    
    m_serv_addr.sin_family = AF_INET;
    
    m_serv_addr.sin_addr.s_addr = inet_addr(m_serv_ipv4_addr.c_str());
    
    m_serv_addr.sin_port = htons(m_serv_portno);
}

Client::ResponseTuple Client::sendRequest(const string& in_command, int in_txn_id, int in_seq_num, const string& in_data, bool in_expectResponse)
{
    string request_header;
    
    request_header.reserve(Constants::request_header_len);
    
    request_header = in_command + Constants::delimiting_character + to_string(in_txn_id) + Constants::delimiting_character + to_string(in_seq_num) + Constants::delimiting_character + to_string(in_data.length());
    
    if (request_header.length() < Constants::request_header_len)
    {
        request_header.push_back(Constants::delimiting_character);
    }
    
    request_header.append(Constants::request_header_len - request_header.length(), Constants::padding_character);
    
#ifdef DEBUG
    if (!Constants::wire_protocol.isValidRequestFormat(request_header.c_str()))
    {
        std::cerr << "Client generated header \"" << request_header << "\" is invalid" << std::endl;
    }
#endif
    
    string request = move(request_header);
    
    request += in_data;
    
    if (ReadWriteHelper::writeFileDescriptor(m_sockfd, request.c_str(), request.length()) < 0)
    {
        perror("Error writing to socket");
        
        exit(EXIT_FAILURE);
    }
    
    return in_expectResponse ? getResponse() : /* else return an empty response  */ ResponseTuple();
}

// ↑                                                                                            ↑ //
// Private Member Functions                                                                       //
////////////////////////////////////////////////////////////////////////////////////////////////////
