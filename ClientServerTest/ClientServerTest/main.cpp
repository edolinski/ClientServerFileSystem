//
//  main.cpp
//  ClientServerTest
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//

#include <iostream>
#include <string>
#include <unordered_map>

#include "argument-helper.h"
#include "gtest/gtest.h"

using std::string;

string g_server_ipv4_addr;

string g_server_port;

string g_server_directory;

using namespace EmersonClientServerFileSystem;

int main(int argc, char *argv[])
{
    srand(static_cast<unsigned int>(time(nullptr)));
    
    bool help = ArgumentHelper::hasHelpArgument(argc, argv);
    
    if (2 >= argc || help)
    {
        ArgumentHelper::printClientHelpMessage();
        
        if (!help)
        {
            exit(EXIT_FAILURE);
        }
        // else let help argument propagate to Google Test below
    }
    else
    {
        std::unordered_map<string, string&> supported_arguments{{ArgumentHelper::server_ipv4_addr_arg_prefix, g_server_ipv4_addr}, {ArgumentHelper::server_port_arg_prefix, g_server_port}, {ArgumentHelper::server_directory_arg_prefix, g_server_directory}};
        
        ArgumentHelper::extractArguments(argc, argv, supported_arguments);
        
        ArgumentHelper::validateIPv4Address(g_server_ipv4_addr);
        
        ArgumentHelper::validatePortNumber(g_server_port);
        
        if (!g_server_directory.empty()) // if optional directory argument specified, validate it
        {
            ArgumentHelper::validateDirectory(g_server_directory);
        }
    }
    
    testing::InitGoogleTest(&argc, argv);
    
    return RUN_ALL_TESTS();
}
