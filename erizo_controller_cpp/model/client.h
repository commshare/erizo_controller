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
    std::string ip;
    uint16_t port;
    std::string family;
    std::string reply_to;

    std::string toJSON() const
    {
        Json::Value root;
        root["id"] = id;
        root["agent_id"] = agent_id;
        root["erizo_id"] = erizo_id;
        root["room_id"] = room_id;
        root["ip"] = ip;
        root["port"] = port;
        root["family"] = family;
        root["reply_to"] = reply_to;
        Json::FastWriter writer;
        return writer.write(root);
    }

    static int fromJSON(const std::string &json, Client &client)
    {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(json, root))
            return 1;

        if (!root.isMember("id") ||
            root["id"].type() != Json::stringValue ||
            !root.isMember("agent_id") ||
            root["agent_id"].type() != Json::stringValue ||
            !root.isMember("erizo_id") ||
            root["erizo_id"].type() != Json::stringValue ||
            !root.isMember("room_id") ||
            root["room_id"].type() != Json::stringValue ||
            !root.isMember("ip") ||
            root["ip"].type() != Json::stringValue ||
            !root.isMember("port") ||
            root["port"].type() != Json::intValue ||
            !root.isMember("family") ||
            root["family"].type() != Json::stringValue ||
            !root.isMember("reply_to") ||
            root["reply_to"].type() != Json::stringValue)
            return 1;

        client.id = root["id"].asString();
        client.agent_id = root["agent_id"].asString();
        client.erizo_id = root["erizo_id"].asString();
        client.room_id = root["room_id"].asString();
        client.ip = root["ip"].asString();
        client.port = root["port"].asInt();
        client.family = root["family"].asString();
        client.reply_to = root["reply_to"].asString();
        return 0;
    }
};

#endif