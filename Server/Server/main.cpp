//
//  main.cpp
//  Server
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//

// TODO: use c++17 filesystem when supported on Xcode

// TODO: mark methods that are const const same with noexcept

#include <mutex>

#include "argument-helper.h"
#include "server.h"
#include "signal-handler.h"

using std::stoi;

using std::string;

std::mutex g_mtx;

using namespace EmersonClientServerFileSystem;

int main(int argc, char *argv[])
{
    srand(static_cast<unsigned int>(time(nullptr)));
    
    if (3 >= argc || ArgumentHelper::hasHelpArgument(argc, argv))
    {
        ArgumentHelper::printServerHelpMessage();
    }
    else
    {
        string server_ipv4_addr;
        
        string server_port;
        
        string server_directory;
        
        std::unordered_map<string, string&> supported_arguments{{ArgumentHelper::server_ipv4_addr_arg_prefix, server_ipv4_addr}, {ArgumentHelper::server_port_arg_prefix, server_port}, {ArgumentHelper::server_directory_arg_prefix, server_directory}};
        
        ArgumentHelper::extractArguments(argc, argv, supported_arguments);
        
        ArgumentHelper::validateIPv4Address(server_ipv4_addr);
        
        ArgumentHelper::validatePortNumber(server_port);
        
        ArgumentHelper::validateDirectory(server_directory);
        
        SignalHandler signal_handler(server_directory);
        
        Server server(server_ipv4_addr, stoi(server_port), server_directory);
        
        server.start();
    }
    
    return 0;
}
