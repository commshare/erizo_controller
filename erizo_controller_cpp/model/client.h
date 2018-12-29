#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <vector>

#include "publisher.h"
#include "subscriber.h"

struct Client
{
    std::string id;
    std::string agent_id;
    std::string erizo_id;
    std::string room_id;
    std::string client_ip;
    uint16_t client_port;
    std::vector<Publisher> publishers;
    std::vector<Subscriber> subscribers;
};

#endif