//
//  constants.h
//  ClientServerShared
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//

#ifndef constants_h
#define constants_h

#include "wire-protocol.h"

namespace EmersonClientServerFileSystem
{
    namespace Constants
    {
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Server Configuration                                                                   //
        // ↓                                                                                    ↓ //
        
        static const int server_backlog = 1'000;
        
        static const int max_sockfd = 255;
        
        static const time_t connection_timeout_seconds = 10;
        
        static const time_t transaction_timeout_seconds = 15;
        
        // ↑                                                                                    ↑ //
        // Server Configuration                                                                   //
        ////////////////////////////////////////////////////////////////////////////////////////////
        
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Message Format                                                                         //
        // ↓                                                                                    ↓ //
        
        static const int default_txn_id = -1;
        
        // Note: WireProtocol currently only supports space delimited request/response pairs
        static const char delimiting_character = ' ';
        
        static const int error_seq_num = -1;
        
        static const int initial_seq_num = 0;
        
        static const char padding_character = '0';
        
        static const int request_header_len = 64;
        
        static const int response_header_len = 128;
        
        // Request Format: COMMAND TXN_ID SEQ_NUM CONTENT_LEN DATA
        
        // ^(?=.{<request_header_len>}$)[A-Z_]+[<delimiting_character>][-]?[0-9]+
        // [<delimiting_character>][-]?[0-9]+[<delimiting_character>][0-9]+
        // ([<delimiting_character>]<padding_character>*)?
        
        static const std::string request_format = std::string("^(?=.{") + std::to_string(request_header_len) + std::string("}$)[A-Z_]+[") + delimiting_character + std::string("][-]?[0-9]+[") + delimiting_character + std::string("][-]?[0-9]+[") + delimiting_character + std::string("][0-9]+([") + delimiting_character + std::string("]") + padding_character + std::string("*)?");
        
        // Response Format: COMMAND TXN_ID SEQ_NUM ERROR_CODE CONTENT_LEN DATA

        // ^(?=.{<response_header_len>}$)[A-Z_]+[<delimiting_character>][-]?[0-9]+
        // [<delimiting_character>][-]?[0-9]+[<delimiting_character>][0-9]+
        // [<delimiting_character>][0-9]+([<delimiting_character>]<padding_character>*)?
        
        static const std::string response_format = std::string("^(?=.{") + std::to_string(response_header_len) + std::string("}$)[A-Z_]+[") + delimiting_character + std::string("][-]?[0-9]+[") + delimiting_character + std::string("][-]?[0-9]+[") + delimiting_character + std::string("][0-9]+[") + delimiting_character + std::string("][0-9]+([") + delimiting_character + std::string("]") + padding_character + std::string("*)?");
        
        static const WireProtocol wire_protocol(request_format.c_str(), response_format.c_str());
        
        // ↑                                                                                    ↑ //
        // Message Format                                                                         //
        ////////////////////////////////////////////////////////////////////////////////////////////
        
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Client Commands                                                                        //
        // ↓                                                                                    ↓ //
        
        static const char * abort_cmd = "ABORT";
        
        static const char * commit_cmd = "COMMIT";
        
        static const char * new_txn_cmd = "NEW_TXN";
        
        static const char * read_cmd = "READ";
        
        static const char * write_cmd = "WRITE";
        
        // ↑                                                                                    ↑ //
        // Client Commands                                                                        //
        ////////////////////////////////////////////////////////////////////////////////////////////
        
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Server Commands                                                                        //
        // ↓                                                                                    ↓ //
        
        static const char * ack_cmd = "ACK";
        
        static const char * ask_resend_cmd = "ASK_RESEND";
        
        static const char * error_cmd = "ERROR";
        
        // ↑                                                                                    ↑ //
        // Server Commands                                                                        //
        ////////////////////////////////////////////////////////////////////////////////////////////
    }
}

#endif /* constants_h */
