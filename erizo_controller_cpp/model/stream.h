#ifndef STREAM_H
#define STREAM_H

#include <string>

#include <json/json.h>

struct Stream
{
    std::string id;
    uint32_t video_ssrc;
    uint32_t audio_ssrc;

    std::string toJSON() const
    {
        Json::Value root;
        root["id"] = id;
        root["video_ssrc"] = video_ssrc;
        root["audio_ssrc"] = audio_ssrc;
        Json::FastWriter writer;
        return writer.write(root);
    }

    static int fromJSON(const std::string &json, Stream &stream)
    {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(json, root))
            return 1;

        if (!root.isMember("id") ||
            root["id"].type() != Json::stringValue ||
            !root.isMember("video_ssrc") ||
            // root["video_ssrc"].type() != Json::intValue ||
            !root.isMember("audio_ssrc")) //||
            // root["audio_ssrc"].type() != Json::intValue)
            return 1;

        stream.id = root["id"].asString();
        stream.video_ssrc = root["video_ssrc"].asUInt();
        stream.audio_ssrc = root["audio_ssrc"].asUInt();
        return 0;
    }
};

#endif