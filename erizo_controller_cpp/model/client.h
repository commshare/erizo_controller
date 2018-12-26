#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <vector>
#include <memory>

#include "publisher.h"
#include "subscriber.h"
#include "websocket/server.hpp"
#include "websocket/client_handler.hpp"

struct Client
{
    std::string id;
    std::string agent_id;
    std::string erizo_id;
    std::string room_id;
    std::string ip;
    uint16_t port;
    std::string agent_ip;
    uint16_t agent_port;
    void *plain_client_hdl;
    void *ssl_client_hdl;

    std::vector<Publisher> publishers;
    std::vector<Subscriber> subscriber;

    Client()
    {
        id = "";
        agent_id = "";
        erizo_id = "";
        room_id = "";
        ip = "";
        port = 0;
        agent_ip = "";
        agent_port = 0;
        plain_client_hdl = nullptr;
        ssl_client_hdl = nullptr;
    }
};

#endif