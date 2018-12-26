#ifndef STREAM_H
#define STREAM_H

#include <string>
#include <memory>

#include "websocket/server.hpp"
#include "websocket/client_handler.hpp"

struct Stream
{
    std::string id;
    std::string erizo_id;
    std::string agent_id;
    void *plain_client_hdl;
    void *ssl_client_hdl;
    
    Stream()
    {
        id = "";
        erizo_id = "";
        agent_id = "";
        plain_client_hdl = nullptr;
        ssl_client_hdl = nullptr;
    }

};

#endif