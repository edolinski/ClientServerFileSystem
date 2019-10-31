//
//  server.cpp
//  Server
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//

#include "constants.h"
#include "server.h"

using namespace EmersonClientServerFileSystem;

Server::Server(const string& in_ipv4_address, int in_portno, const string& in_directory) : m_up_dispatcher(make_unique<ServerDispatcher<ServerBackend>>(in_ipv4_address, in_portno, Constants::server_backlog, Constants::max_sockfd, Constants::connection_timeout_seconds, make_unique<ServerBackend>(in_directory))) {}

void Server::start()
{
    m_up_dispatcher->start();
}
