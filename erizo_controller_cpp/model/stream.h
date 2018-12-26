#ifndef STREAM_H
#define STREAM_H

#include <string>
#include <memory>

struct Stream
{
    std::string id;
    std::string erizo_id;
    std::string agent_id;
    
    Stream()
    {
        id = "";
        erizo_id = "";
        agent_id = "";
    }

};

#endif