#ifndef BRIDGE_STREAM_H
#define BRIDGE_STREAM_H

#include <string>

#include <json/json.h>

struct BridgeStream
{
    std::string id;
    std::string sender_erizo_id;
    std::string sender_ip;
    uint16_t sender_port;
    std::string recver_erizo_id;
    std::string recver_ip;
    uint16_t recver_port;
    std::string src_stream_id;
    std::string label;
    int subscribe_count;

    std::string toJSON() const
    {
        Json::Value root;
        root["id"] = id;
        root["sender_erizo_id"] = sender_erizo_id;
        root["sender_ip"] = sender_ip;
        root["sender_port"] = sender_port;
        root["recver_erizo_id"] = recver_erizo_id;
        root["recver_ip"] = recver_ip;
        root["recver_port"] = recver_port;
        root["src_stream_id"] = src_stream_id;
        root["label"] = label;
        root["subscribe_count"] = subscribe_count;
        Json::FastWriter writer;
        return writer.write(root);
    }

    static int fromJSON(const std::string &json, BridgeStream &bridge_stream)
    {
        Json::Value root;
        Json::Reader reader(Json::Features::strictMode());
        if (!reader.parse(json, root))
            return 1;
        if (!root.isMember("id") ||
            root["id"].type() != Json::stringValue ||
            !root.isMember("sender_erizo_id") ||
            root["sender_erizo_id"].type() != Json::stringValue ||
            !root.isMember("sender_ip") ||
            root["sender_ip"].type() != Json::stringValue ||
            !root.isMember("sender_port") ||
            root["sender_port"].type() != Json::intValue ||
            !root.isMember("recver_erizo_id") ||
            root["recver_erizo_id"].type() != Json::stringValue ||
            !root.isMember("recver_ip") ||
            root["recver_ip"].type() != Json::stringValue ||
            !root.isMember("recver_port") ||
            root["recver_port"].type() != Json::intValue ||
            !root.isMember("src_stream_id") ||
            root["src_stream_id"].type() != Json::stringValue ||
            !root.isMember("label") ||
            root["label"].type() != Json::stringValue ||
            !root.isMember("subscribe_count") ||
            root["subscribe_count"].type() != Json::intValue)
            return 1;

        bridge_stream.id = root["id"].asString();
        bridge_stream.sender_erizo_id = root["sender_erizo_id"].asString();
        bridge_stream.sender_ip = root["sender_ip"].asString();
        bridge_stream.sender_port = root["sender_port"].asInt();
        bridge_stream.recver_erizo_id = root["recver_erizo_id"].asString();
        bridge_stream.recver_ip = root["recver_ip"].asString();
        bridge_stream.recver_port = root["recver_port"].asInt();
        bridge_stream.src_stream_id = root["src_stream_id"].asString();
        bridge_stream.label = root["label"].asString();
        bridge_stream.subscribe_count = root["subscribe_count"].asInt();
        return 0;
    };
};

#endif