//
//  server-dispatcher.h
//  Server
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// The ServerDispatcher class serves as the middleman between clients and the backend server.     //
// Incoming connections from clients are accepted by the ServerDispatcher where the client        //
// request is extracted and passed to the backend server for processing. Afer processing, the     //
// backend server sets a response for the ServerDispatcher to send back to the client.            //
//                                                                                                //
// Note: ServerDispatcher is multithreaded, spawning a thread to handle each incoming connection. //
//                                                                                                //
// Note: ServerDispatcher will terminate a connection if m_connection_timeout_seconds elapse      //
//       without receiving a packet.                                                              //
//                                                                                                //
// Note: ServerDispatcher is templatized on ServerBackend to support greater flexibility and      //
//       reusability. All that is required is for ServerBackend to implement:                     //
//                                                                                                //
//       int getContentLength(const char * in_request_header, string& out_server_response_str,    //
//                            bool& out_transaction_in_progress);                                 //
//                                                                                                //
//       int getRequestHeaderLength();                                                            //
//                                                                                                //
//       void processRequest(const char * in_request_header, const char * in_request_payload,     //
//                           string& out_server_response_str, bool& out_transaction_in_progress); //
//                                                                                                //
////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: use a thread pool

// TODO: handle case where m_max_sockfd exceeds FD_SETSIZE

#ifndef server_dispatcher_h
#define server_dispatcher_h

#include <functional>
#ifdef DEBUG
#include <iostream>
#endif
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include "read-write-helper.h"

extern std::mutex g_mtx;

namespace EmersonClientServerFileSystem
{
    template<class ServerBackend>
    class ServerDispatcher
    {
        
    private:
        
        using condition_variable = std::condition_variable;
        
        using mutex = std::mutex;
        
        using string = std::string;
        
        using thread = std::thread;
        
        template<class T>
        using function = std::function<T>;
        
        template<class T>
        using lock_guard = std::lock_guard<T>;
        
        template<class T>
        using unique_lock = std::unique_lock<T>;
        
        template<class T>
        using unique_ptr = std::unique_ptr<T>;
        
        template<class T>
        using vector = std::vector<T>;
        
        int m_backlog;
        
        int m_listenfd;
        
        int m_portno;
        
        int m_sockfd;
        
        int m_max_sockfd;
        
        const time_t m_connection_timeout_seconds;
        
        const string m_ipv4_addr;
        
        socklen_t m_cli_len;
        
        struct sockaddr_in m_cli_addr;

        // unique_ptr to handle case where ServerBackend is not copyable/movable
        unique_ptr<ServerBackend> m_up_backend;
        
        function<void(int)> m_processRequest;
        
        function<void()> m_notifier;
        
        vector<condition_variable> m_fd_cvs;
        
        vector<mutex> m_fd_mtxs;
        
        fd_set m_rfds;
        
        void initializeConditionVariablesAndMutexes();
        
        void initializeNotifier();
        
        void initializeProcessRequest();
        
        void initializeSocket();
        
        bool readSocket(int in_sockfd, char * out_buffer, int in_buffer_len);
        
    public:
        
        ServerDispatcher(const string& in_ipv4_addr, int in_portno, int in_backlog, int in_max_sockfd, time_t in_connection_timeout_seconds, unique_ptr<ServerBackend>&& in_up_backend);
        
        void start();
        
    };
    
    template<class ServerBackend>
    ServerDispatcher<ServerBackend>::ServerDispatcher(const string& in_ipv4_addr, int in_portno, int in_backlog, int in_max_sockfd, time_t in_connection_timeout_seconds, unique_ptr<ServerBackend>&& in_up_backend) : m_ipv4_addr(in_ipv4_addr), m_portno(in_portno), m_backlog(in_backlog), m_max_sockfd(in_max_sockfd), m_connection_timeout_seconds(in_connection_timeout_seconds), m_up_backend(move(in_up_backend))
    {
        FD_ZERO(&m_rfds);
        
        initializeConditionVariablesAndMutexes();
        
        initializeNotifier();
        
        initializeProcessRequest();
        
        initializeSocket();
    }
    
    template<class ServerBackend>
    void ServerDispatcher<ServerBackend>::start()
    {
        thread(m_notifier).detach();
        
        while (1)
        {
            unique_lock<mutex> fd_lck(m_fd_mtxs[m_listenfd]);
            
            m_fd_cvs[m_listenfd].wait(fd_lck);
            
            unique_lock<mutex> global_lck(g_mtx);
            
            m_sockfd = accept(m_listenfd, (struct sockaddr *) &m_cli_addr, &m_cli_len);
            
            if (m_sockfd < 0)
            {
                perror("Error on accept");
                
                exit(EXIT_FAILURE);
            }
            
            if (m_sockfd > m_max_sockfd)
            {
#ifdef DEBUG
                std::cerr << "Maximum connections reached... closing connection" << std::endl;
#endif
                close(m_sockfd);
                
                continue;
            }
            
            FD_SET(m_sockfd, &m_rfds);
            
            global_lck.unlock();
            
#ifdef DEBUG
            global_lck.lock();
            
            std::cout << "opened socket descriptor " << m_sockfd << std::endl;
            
            global_lck.unlock();
#endif
            
            thread(m_processRequest, m_sockfd).detach();
        }
        
        close(m_listenfd);
        
        FD_CLR(m_listenfd, &m_rfds);
    }
    
    template<class ServerBackend>
    void ServerDispatcher<ServerBackend>::initializeConditionVariablesAndMutexes()
    {
        decltype(m_fd_cvs) tmp_fd_cvs(m_max_sockfd + 1);
        
        swap(m_fd_cvs, tmp_fd_cvs);
        
        decltype(m_fd_mtxs) tmp_fd_mtxs(m_max_sockfd + 1);
        
        swap(m_fd_mtxs, tmp_fd_mtxs);
    }
    
    template<class ServerBackend>
    void ServerDispatcher<ServerBackend>::initializeNotifier()
    {
        m_notifier = [this]()
        {
            fd_set rfds;
            
            struct timeval tv;
            
            int nfds = m_max_sockfd + 1;
            
            while (1)
            {
                tv.tv_sec = 0;
                
                tv.tv_usec = 100'000;
                
                unique_lock<mutex> global_lck(g_mtx);
                
                rfds = m_rfds;
                
                int numReadyFileDescriptors = select(nfds, &rfds, nullptr, nullptr, &tv);
                
                if (numReadyFileDescriptors < 0)
                {
#ifdef DEBUG
                    perror("Error on select");
#endif
                }
                
                global_lck.unlock();
                
                for (int fd = m_listenfd; numReadyFileDescriptors > 0 && fd <= m_max_sockfd; ++fd)
                {
                    if (FD_ISSET(fd, &rfds))
                    {
                        m_fd_cvs[fd].notify_one();
                        
                        --numReadyFileDescriptors;
                    }
                }
            }
        };
    }
    
    template<class ServerBackend>
    void ServerDispatcher<ServerBackend>::initializeProcessRequest()
    {
        m_processRequest = [this](int in_sockfd)
        {
#ifdef DEBUG
            if (unique_lock<mutex> global_lck(g_mtx); global_lck.owns_lock())
            {
                std::cout << "processing socket descriptor " << in_sockfd << std::endl;
            }
#endif
            
            bool transaction_in_progress = true;
            
            do
            {
                int request_header_len = m_up_backend->getRequestHeaderLength();
                
                char request_header[request_header_len + 1]; // add 1 for null terminator
                
                bzero(request_header, request_header_len + 1);
                
                if (readSocket(in_sockfd, request_header, request_header_len))
                {
                    string server_response;
                    
                    int content_len = m_up_backend->getContentLength(request_header, server_response, transaction_in_progress);
                    
                    if (content_len < 0)
                    {
                        if (server_response.length() > 0 && ReadWriteHelper::writeFileDescriptor(in_sockfd, server_response.c_str(), server_response.length()) < 0)
                        {
#ifdef DEBUG
                            perror("Error writing to socket file descriptor");
#endif
                        }
                    }
                    else
                    {
                        char request_payload[content_len + 1]; // add 1 for null terminator
                        
                        bzero(request_payload, content_len + 1);
                        
                        if (readSocket(in_sockfd, request_payload, content_len))
                        {
                            m_up_backend->processRequest(request_header, request_payload, server_response, transaction_in_progress);
                            
                            if (server_response.length() > 0 && ReadWriteHelper::writeFileDescriptor(in_sockfd, server_response.c_str(), server_response.length()) < 0)
                            {
#ifdef DEBUG
                                perror("Error writing to socket file descriptor");
#endif
                            }
                        }
                        else // read failed
                        {
                            break;
                        }
                    }
                }
                else // read failed
                {
                    break;
                }
            }
            while (transaction_in_progress);
            
            lock_guard<mutex> global_grd(g_mtx);
            
#ifdef DEBUG
            std::cout << "closing socket descriptor " << in_sockfd << std::endl;
#endif
            
            if (close(in_sockfd) < 0)
            {
#ifdef DEBUG
                perror("Error closing socket descriptor");
#endif
            }
            
            FD_CLR(in_sockfd, &m_rfds);
        };
    }
    
    template<class ServerBackend>
    void ServerDispatcher<ServerBackend>::initializeSocket()
    {
        struct sockaddr_in serv_addr;
        
        m_listenfd = socket(AF_INET, SOCK_STREAM, 0);
        
        FD_SET(m_listenfd, &m_rfds);
        
        if (m_listenfd < 0)
        {
            perror("Error opening socket");
            
            exit(EXIT_FAILURE);
        }
        
        bzero((char *) &serv_addr, sizeof(serv_addr));
        
        serv_addr.sin_family = AF_INET;
        
        serv_addr.sin_addr.s_addr = inet_addr(m_ipv4_addr.c_str());
        
        serv_addr.sin_port = htons(m_portno);
        
        if (bind(m_listenfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        {
            perror("Error on binding");
            
            exit(EXIT_FAILURE);
        }
        
        listen(m_listenfd, m_backlog);
        
        m_cli_len = sizeof(m_cli_addr);
    }
    
    template<class ServerBackend>
    bool ServerDispatcher<ServerBackend>::readSocket(int in_sockfd, char * out_buffer, int in_buffer_len)
    {
        assert(!(nullptr == out_buffer));
        
        if (ReadWriteHelper::readFileDescriptor(in_sockfd, out_buffer, in_buffer_len, m_connection_timeout_seconds, &m_fd_mtxs[in_sockfd], &m_fd_cvs[in_sockfd]) < 0)
        {
            if (!(ECONNRESET == errno || ENOENT == errno || ETIMEDOUT == errno || 0 == errno))
            {
#ifdef DEBUG
                // ignore errors related to client closing connection and client timeout as these are
                // errors on the client-side
                if (unique_lock<mutex> global_lck(g_mtx); global_lck.owns_lock())
                {
                    perror("Error reading from socket file descriptor");
                }
#endif
            }
            
            return false;
        }
        
        return true;
    }
}

#endif /* server_dispatcher_h */
