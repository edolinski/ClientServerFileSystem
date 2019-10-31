//
//  signal-handler.h
//  Server
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//

// TODO: erase only server created files when ctrl-c signal received

#ifndef signal_handler_h
#define signal_handler_h

#include <dirent.h>
#include <signal.h>

namespace EmersonClientServerFileSystem
{
    class SignalHandler
    {
        
    private:
        
        static std::string s_directory;
        
        static void ctrlc(int s)
        {
            std::string input;
            
            std::cout << std::endl;
            
            do
            {
                std::cout << "Erase all directory contents (Y/N)?: ";
                
                std::cin >> input;
                
                if ("Y" == input)
                {
                    do
                    {
                        std::cout << "Are you sure (Y/N)?: ";
                        
                        std::cin >> input;
                        
                        if ("Y" == input)
                        {
                            auto dir = opendir(s_directory.c_str());
                            
                            struct dirent * next_file;
                            
                            char filepath[256];
                            
                            while (!(nullptr == (next_file = readdir(dir))))
                            {
                                sprintf(filepath, "%s/%s", s_directory.c_str(), next_file->d_name);
                                
                                remove(filepath);
                            }
                            
                            closedir(dir);
                        }
                    }
                    while (!("Y" == input || "N" == input));
                }
            }
            while (!("Y" == input || "N" == input));
            
            exit(EXIT_SUCCESS);
        }
        
    public:
        
        SignalHandler(const std::string& in_directory)
        {
            s_directory = in_directory;
            
            struct sigaction sigIntHandler;
            
            sigIntHandler.sa_handler = ctrlc;
            
            sigemptyset(&sigIntHandler.sa_mask);
            
            sigIntHandler.sa_flags = 0;
            
            sigaction(SIGINT, &sigIntHandler, nullptr);
        }
        
    };
}

std::string EmersonClientServerFileSystem::SignalHandler::s_directory;

#endif /* signal_handler_h */
