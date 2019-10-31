//
//  server.h
//  Server
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// The Server class is used as a simple wrapper around ServerDispatcher to abstract the details   //
// of construction away from main. Only the arguments obtained in main are left exposed in the    //
// Server constructor.                                                                            //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef server_h
#define server_h

#include "server-backend.h"
#include "server-dispatcher.h"

namespace EmersonClientServerFileSystem
{
    class Server
    {
        
    private:
        
        using string = std::string;
        
        template<class T>
        using unique_ptr = std::unique_ptr<T>;
        
        template<class T>
        static constexpr auto make_unique = [](auto&&... ts) constexpr -> decltype(auto) { return std::make_unique<T>(std::forward<decltype(ts)>(ts)...);};
        
        // unique_ptr as ServerDispatcher contains mutexes and condition_variables which are not
        // copyable/movable
        unique_ptr<ServerDispatcher<ServerBackend>> m_up_dispatcher;
        
    public:
        
        Server(const string& in_ipv4_address, int in_portno, const string& in_directory);
        
        void start();
        
    };
}

#endif /* server_h */
