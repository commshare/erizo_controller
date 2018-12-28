#ifndef PUBLISHER_H
#define PUBLISHER_H

#include <string>
#include <vector>

#include <json/json.h>

#include "stream.h"

struct Publisher : public Stream
{
    std::string toJSON() const
    {
        Json::Value root;
        root["id"] = id;
        root["erizo_id"] = erizo_id;
        root["agent_id"] = agent_id;
        root["label"] = label;
        Json::FastWriter writer;
        return writer.write(root);
    }

    static int fromJSON(const std::string &json, Publisher &publisher)
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
            root["label"].type() != Json::stringValue)
            return 1;

        publisher.id = root["id"].asString();
        publisher.erizo_id = root["erizo_id"].asString();
        publisher.agent_id = root["agent_id"].asString();
        publisher.label = root["label"].asString();
        return 0;
    }
};
#endif