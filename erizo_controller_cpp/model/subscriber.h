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
    std::string subscribe_to;
    std::string reply_to;

    std::string toJSON() const
    {
        Json::Value root;
        root["id"] = id;
        root["agent_id"] = agent_id;
        root["erizo_id"] = erizo_id;
        root["client_id"] = client_id;
        root["is_bridge"] = is_bridge;
        root["remote_erizo_id"] = remote_erizo_id;
        root["remote_agent_id"] = remote_agent_id;
        root["subscribe_to"] = subscribe_to;
        root["reply_to"] = reply_to;
        Json::FastWriter writer;
        return writer.write(root);
    }

    static int fromJSON(const std::string &json, Subscriber &subscriber)
    {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(json, root))
            return 1;

        if (!root.isMember("id") ||
            root["id"].type() != Json::stringValue ||
            !root.isMember("client_id") ||
            root["client_id"].type() != Json::stringValue ||
            !root.isMember("erizo_id") ||
            root["erizo_id"].type() != Json::stringValue ||
            !root.isMember("agent_id") ||
            root["agent_id"].type() != Json::stringValue ||
            !root.isMember("is_bridge") ||
            root["is_bridge"].type() != Json::booleanValue ||
            !root.isMember("remote_erizo_id") ||
            root["remote_erizo_id"].type() != Json::stringValue ||
            !root.isMember("remote_agent_id") ||
            root["remote_agent_id"].type() != Json::stringValue ||
            !root.isMember("subscribe_to") ||
            root["subscribe_to"].type() != Json::stringValue ||
            !root.isMember("reply_to") ||
            root["reply_to"].type() != Json::stringValue)
            return 1;

        subscriber.id = root["id"].asString();
        subscriber.client_id = root["client_id"].asString();
        subscriber.erizo_id = root["erizo_id"].asString();
        subscriber.agent_id = root["agent_id"].asString();
        subscriber.is_bridge = root["is_bridge"].asBool();
        subscriber.remote_erizo_id = root["remote_erizo_id"].asString();
        subscriber.remote_agent_id = root["remote_agent_id"].asString();
        subscriber.subscribe_to = root["subscribe_to"].asString();
        subscriber.reply_to = root["reply_to"].asString();
        return 0;
    }
};

#endif