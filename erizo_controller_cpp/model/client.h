#ifndef CLIENT_H
#define CLIENT_H

#include <string>

struct Client
{
    std::string id;
    std::string room_id;
    std::string agent_id;
    std::string erizo_id;
};

#endif