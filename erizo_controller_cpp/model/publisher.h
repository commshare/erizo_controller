#ifndef PUBLISHER_H
#define PUBLISHER_H

#include <string>
#include <vector>

#include <json/json.h>

struct Publisher
{
    std::string id;
    std::string client_id;
    std::string erizo_id;
    std::string bridge_ip;
    uint16_t bridge_port;
    std::string agent_id;
    std::string label;
    uint32_t video_ssrc;
    uint32_t audio_ssrc;

    std::string toJSON() const
    {
        Json::Value root;
        root["id"] = id;
        root["erizo_id"] = erizo_id;
        root["bridge_ip"] = bridge_ip;
        root["bridge_port"] = bridge_port;
        root["agent_id"] = agent_id;
        root["client_id"] = client_id;
        root["label"] = label;
        root["video_ssrc"] = video_ssrc;
        root["audio_ssrc"] = audio_ssrc;
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
            !root.isMember("bridge_ip") ||
            root["bridge_ip"].type() != Json::stringValue ||
            !root.isMember("bridge_port") ||
            root["bridge_port"].type() != Json::intValue ||
            !root.isMember("agent_id") ||
            root["agent_id"].type() != Json::stringValue ||
            !root.isMember("client_id") ||
            root["client_id"].type() != Json::stringValue ||
            !root.isMember("label") ||
            root["label"].type() != Json::stringValue ||
            !root.isMember("video_ssrc") ||
            !root.isMember("audio_ssrc"))
            return 1;

        publisher.id = root["id"].asString();
        publisher.erizo_id = root["erizo_id"].asString();
        publisher.bridge_ip = root["bridge_ip"].asString();
        publisher.bridge_port = root["bridge_port"].asInt();
        publisher.agent_id = root["agent_id"].asString();
        publisher.client_id = root["client_id"].asString();
        publisher.label = root["label"].asString();
        publisher.video_ssrc = root["video_ssrc"].asUInt();
        publisher.audio_ssrc = root["audio_ssrc"].asUInt();

        return 0;
    }
};
#endif