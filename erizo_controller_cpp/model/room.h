#ifndef ROOM_H
#define ROOM_H

#include <string>

#include <json/json.h>

struct Room
{
    std::string id;
    std::string name;

    std::string toJSON() const
    {
        Json::Value root;
        root["id"] = id;
        root["name"] = name;

        Json::FastWriter writer;
        return writer.write(root);
    }

    static int fromJSON(const std::string &json, Room &room)
    {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(json, root))
            return 1;

        if (!root.isMember("id") ||
            root["id"].type() != Json::stringValue ||
            !root.isMember("name") ||
            root["name"].type() != Json::stringValue)
            return 1;

        room.id = root["id"].asString();
        room.name = root["name"].asString();
        return 0;
    }
};

#endif