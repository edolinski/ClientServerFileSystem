//
//  client.h
//  ClientServerTest
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// The Client class is used to abstract away the details of setting up a connection with the      //
// server as well as to provide functionality for sending and receiving messages to/from the      //
// server.                                                                                        //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef client_h
#define client_h

#include <iostream>
#include <string>

#include <netinet/in.h>
#include <sys/socket.h>

namespace EmersonClientServerFileSystem
{
    class Client
    {
        
    private:
        
        using string = std::string;
        
        using ResponseTuple = std::tuple<string, int, int, int, int, string>;
        
        static constexpr auto to_string = [](auto t) constexpr -> decltype(auto) { return std::to_string(t);};
        
        int m_sockfd;
        
        string m_serv_ipv4_addr;
        
        int m_serv_portno;
        
        socklen_t m_serv_len;
        
        struct sockaddr_in m_serv_addr;
        
        void connectToServer();
        
        void initializeSocket();
        
        ResponseTuple sendRequest(const string& in_command, int in_txn_id, int in_seq_num, const string& in_data, bool in_expectResponse);
        
    public:
        
        Client(string in_serv_ipv4_addr, int in_serv_portno);
        
        ~Client();
        
        ResponseTuple getResponse();
        
        static void printResponse(const ResponseTuple& in_server_response_tuple);
        
        void sendRequest(const string& in_command, int in_txn_id, int in_seq_num, const string& in_data = "");
        
        ResponseTuple sendRawRequestGetResponse(const char * in_raw_request, size_t in_raw_request_len);
        
        ResponseTuple sendRequestGetResponse(const string& in_command, int in_txn_id, int in_seq_num, const string& in_data = "");
    };
}

#endif /* client_h */
