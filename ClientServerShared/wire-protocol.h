//
//  wire-protocol.h
//  ClientServerShared
//
//  Created by Emerson Dolinski.
//  Copyright © 2019 Emerson Dolinski. All rights reserved.
//
////////////////////////////////////////////////////////////////////////////////////////////////////
// The WireProtocol class is used to validate requests and responses according to their           //
// associated regex. WireProtocol is also used to extract the relevant fields from a raw header   //
// string and place them inside a tuple-like data structure. The current implementation only      //
// supports parsing of strings where each field is delimited by a space character.                //
////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef wire_protocol_h
#define wire_protocol_h

#include <regex>
#include <sstream>
#include <tuple>

namespace EmersonClientServerFileSystem
{
    class WireProtocol
    {
        
    private:
        
        using istringstream = std::istringstream;
        
        using regex = std::regex;
        
        using string = std::string;
        
        template<class T>
        using tuple_size = std::tuple_size<T>;
        
        regex m_request_regex;
        
        regex m_response_regex;
        
        ////////////////////////////////////////////////////////////////////////////////////////////
        // Credit to Stack Overflow @Jean-Michaël Celerier https://bit.ly/2kUTIq2                 //
        ////////////////////////////////////////////////////////////////////////////////////////////
        template<std::size_t N>
        struct num { static const constexpr auto value = N; };
        
        template <class F, std::size_t... Is>
        void for_(F func, std::index_sequence<Is...>) const
        {
            using expander = int[];
            (void)expander{0, ((void)func(num<Is>{}), 0)...};
        }
        
        template <std::size_t N, typename F>
        void for_(F func) const
        {
            for_(func, std::make_index_sequence<N>());
        }
        ////////////////////////////////////////////////////////////////////////////////////////////
        
    public:
        
        WireProtocol(const char * in_request_format, const char * in_response_format)
        {
            assert(!(nullptr == in_request_format));
            
            assert(!(nullptr == in_response_format));
            
            m_request_regex = regex(in_request_format);
            
            m_response_regex = regex(in_response_format);
        }
        
        bool isValidRequestFormat(const char * in_header) const
        {
            assert(!(nullptr == in_header));
            
            return regex_match(in_header, m_request_regex);
        }
        
        bool isValidResponseFormat(const char * in_header) const
        {
            assert(!(nullptr == in_header));
            
            return regex_match(in_header, m_response_regex);
        }
        
        template<class MessageTuple>
        void extractHeaderFields(const char * in_header, MessageTuple& out_message) const
        {
            assert(!(nullptr == in_header));
            
            auto iss = istringstream(string(in_header));
            
            constexpr int size = tuple_size<MessageTuple>::value;
            
            for_<size - 1>([&] (auto i) {
                iss >> std::get<i.value>(out_message);
            });
        }
        
    };
}

#endif
