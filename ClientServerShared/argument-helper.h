//
//  argument-helper.h
//  ClientServerShared
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// The ArgumentHelper functions are used to extract and validate command line arguments as well   //
// as print help messages relevant to both the ClientServerTest and Server applications.          //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef argument_helper_h
#define argument_helper_h

#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>

#include <dirent.h>

namespace EmersonClientServerFileSystem
{
    namespace ArgumentHelper
    {
        static const char * server_ipv4_addr_arg_prefix = "--server_ipv4_addr=";
        
        static const char * server_port_arg_prefix = "--server_port=";
        
        static const char * server_directory_arg_prefix = "--server_directory=";
        
        static const char * help_arg_prefix = "--help";
        
        static const char * argument_indent = "  ";
        
        static const char * description_indent = "      ";
        
        static inline void extractArguments(int in_argc, char * in_argv[], std::unordered_map<std::string, std::string&>& in_out_supported_arguments)
        {
            for (int i = 1; i < in_argc; ++i)
            {
                std::string arg = in_argv[i];
                
                for (auto& [prefix, var] : in_out_supported_arguments)
                {
                    if (!arg.compare(0, prefix.size(), prefix))
                    {
                        var = arg.substr(prefix.size());
                        
                        in_argv[i] = nullptr;
                        
                        break;
                    }
                }
            }
        }
        
        static inline bool hasHelpArgument(int in_argc, char * in_argv[])
        {
            for (int i = 1; i < in_argc; ++i)
            {
                if (0 == strcmp(help_arg_prefix, in_argv[i]))
                {
                    return true;
                }
            }
            
            return false;
        }
        
        static inline void printClientHelpMessage()
        {
            std::cout << std::endl;
            
            std::cout << "Required Arguments:" << std::endl;
            
            std::cout << argument_indent << server_ipv4_addr_arg_prefix << "[IPV4_ADDRESS]" << std::endl;
            
            std::cout << description_indent << "The IPv4 address of the server to connect to for running the tests." << std::endl;
            
            std::cout << argument_indent << server_port_arg_prefix << "[NUMBER]" << std::endl;
            
            std::cout << description_indent << "The port number the server file system is listening on." << std::endl;
            
            std::cout << std::endl;
            
            std::cout << "Optional Arguments:" << std::endl;
            
            std::cout << argument_indent << server_directory_arg_prefix << "[DIRECTORY_PATH]" << std::endl;
            
            std::cout << description_indent << "The path to the directory where the server is writing files. This\n" << description_indent << "argument can be used when the client and server are running on the\n" << description_indent << "same machine to enable deletion of test created files." << std::endl;
            
            std::cout << std::endl;
        }
        
        static inline void printServerHelpMessage()
        {
            std::cout << std::endl;
            
            std::cout << "Required Arguments:" << std::endl;
            
            std::cout << argument_indent << server_ipv4_addr_arg_prefix << "[IPV4_ADDRESS]" << std::endl;
            
            std::cout << description_indent << "The IPv4 address of the server." << std::endl;
            
            std::cout << argument_indent << server_port_arg_prefix << "[NUMBER]" << std::endl;
            
            std::cout << description_indent << "The port number the server is to listen on." << std::endl;
            
            std::cout << argument_indent << server_directory_arg_prefix << "[DIRECTORY_PATH]" << std::endl;
            
            std::cout << description_indent << "The path to the directory where the server is to write files." << std::endl;
            
            std::cout << std::endl;
        }
        
        static inline void validateDirectory(std::string& in_directory)
        {
            if (in_directory.empty())
            {
                std::cerr << "Error no server directory provided. Please provide a server directory by passing " << server_directory_arg_prefix << "<server_directory> as a command line argument." << std::endl;
                
                exit(EXIT_FAILURE);
            }
            
            if (auto dir = opendir(in_directory.c_str()); dir)
            {
                closedir(dir); // directory exists, close directory
                
                if (!('/' == in_directory.back()))
                {
                    in_directory.push_back('/');
                }
            }
            else if (ENOENT == errno)
            {
                std::cerr << "Error server directory \"" << in_directory << "\" does not exist" << std::endl;
                
                exit(EXIT_FAILURE);
            }
            else
            {
                std::cerr << "Error opening server directory \"" << in_directory << "\"" << std::endl;
                
                exit(EXIT_FAILURE);
            }
        }
        
        static inline void validateIPv4Address(const std::string& in_ipv4_address)
        {
            static const char * ipv4_address_format = "^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$";
         
            if (in_ipv4_address.empty())
            {
                std::cerr << "Error no server ipv4 address provided. Please provide a server ipv4 address by passing " << server_ipv4_addr_arg_prefix << "<server_ip_addr> as a command line argument." << std::endl;
                
                exit(EXIT_FAILURE);
            }
            
            if (!regex_match(in_ipv4_address, std::regex(ipv4_address_format)))
            {
                std::cerr << "Error invalid IPv4 address \"" << in_ipv4_address << "\". Please provide a four octet address expressed in decimal in the form \"AAA.BBB.CCC.DDD\"." << std::endl;
                
                exit(EXIT_FAILURE);
            }
        }
        
        static inline void validatePortNumber(const std::string& in_portno)
        {
            static const char * port_format = "^([1-9][0-9]{0,3}|[1-5][0-9]{4}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])$";
            
            if (in_portno.empty())
            {
                std::cerr << "Error no server port provided. Please provide a server port by passing " << server_port_arg_prefix << "<server_port> as a command line argument." << std::endl;
                
                exit(EXIT_FAILURE);
            }
            
            if (!regex_match(in_portno, std::regex(port_format)))
            {
                std::cerr << "Error invalid port number \"" << in_portno << "\". Please provide a port number in the range 1..65535." << std::endl;
                
                exit(EXIT_FAILURE);
            }
        }
    }
}

#endif /* argument_helper_h */
