#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <map>

#include "publisher.h"
#include "subscriber.h"

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
    std::map<std::string, Publisher> publishers;
    std::map<std::string, Subscriber> subscribers;

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
    }

    int getStream(const std::string &stream_id, Stream &stream)
    {
        auto publishs_it = publishers.find(stream_id);
        if (publishs_it != publishers.end())
        {
            stream = publishs_it->second;
            return 0;
        }

        auto subscribers_it = subscribers.find(stream_id);
        if (subscribers_it != subscribers.end())
        {
            stream = subscribers_it->second;
            return 0;
        }
        return 1;
    }
};

#endif