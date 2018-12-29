#ifndef SUBSCRIBER_H
#define SUBSCRIBER_H

#include <string>
#include <vector>

#include <json/json.h>

struct Subscriber : public Stream
{
    bool is_bridge;
    std::string remote_erizo_id;
    std::string remote_agent_id;
   
    std::string toJSON() const
    {
        Json::Value root;
        root["id"] = id;
        root["erizo_id"] = erizo_id;
        root["agent_id"] = agent_id;
        root["is_bridge"] = is_bridge;
        root["remote_erizo_id"] = remote_erizo_id;
        root["remote_agent_id"] = remote_agent_id;
        Json::FastWriter writer;
        return writer.write(root);
    }

    static int fromJSON(const std::string &json, Subscriber &subscribe)
    {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(json, root))
            return 1;

        if (!root.isMember("id") ||
            root["id"].type() != Json::stringValue ||
            !root.isMember("erizo_id") ||
            root["erizo_id"].type() != Json::stringValue ||
            !root.isMember("agent_id") ||
            root["agent_id"].type() != Json::stringValue ||
            !root.isMember("label") ||
            root["label"].type() != Json::stringValue ||
            !root.isMember("is_bridge") ||
            root["is_bridge"].type() != Json::booleanValue ||
            !root.isMember("remote_erizo_id") ||
            root["remote_erizo_id"].type() != Json::stringValue ||
            !root.isMember("remote_agent_id") ||
            root["remote_agent_id"].type() != Json::stringValue)
            return 1;

        subscribe.id = root["id"].asString();
        subscribe.erizo_id = root["erizo_id"].asString();
        subscribe.agent_id = root["agent_id"].asString();
        subscribe.is_bridge = root["is_bridge"].asBool();
        subscribe.remote_erizo_id = root["remote_erizo_id"].asString();
        subscribe.remote_agent_id = root["remote_agent_id"].asString();
        return 0;
    }
};

#endif