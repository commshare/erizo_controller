#include "redis_helper.h"

#include <algorithm>

#include "acl_redis.h"

int RedisHelper::addClient(const std::string &room_id, const Client &client)
{
    std::string key = "clients_" + room_id;
    if (ACLRedis::getInstance()->hset(key, client.id, client.toJSON()) == -1)
        return 1;
    return 0;
}

int RedisHelper::removeClient(const std::string &room_id, const std::string &client_id)
{
    std::string key = "clients_" + room_id;
    if (ACLRedis::getInstance()->hdel(key, client_id) == -1)
        return 1;
    return 0;
}

int RedisHelper::getAllClient(const std::string &room_id, std::vector<Client> &clients)
{
    std::string key = "clients_" + room_id;
    std::vector<std::string> fields, values;
    if (ACLRedis::getInstance()->hvals(key, fields, values) == -1)
        return 1;
    clients.clear();
    for (std::string &v : values)
    {
        Client c;
        if (!Client::fromJSON(v, c))
            clients.push_back(c);
    }
    return 0;
}

int RedisHelper::addPublisher(const std::string &room_id, const Publisher &pubilsher)
{
    std::string key = "publishers_" + room_id;
    if (ACLRedis::getInstance()->hset(key, pubilsher.id, pubilsher.toJSON()) == -1)
        return 1;
    return 0;
}

int RedisHelper::getPublisher(const std::string &room_id, const std::string &publisher_id, Publisher &publisher)
{
    std::string key = "publishers_" + room_id;
    std::string buf;
    if (ACLRedis::getInstance()->hget(key, publisher_id, buf) == -1)
        return 1;
    if (Publisher::fromJSON(buf, publisher))
        return 1;

    return 0;
}

int RedisHelper::removePublishers(const std::string &room_id, const std::vector<std::string> &publisher_ids)
{
    std::string key = "publishers_" + room_id;
    if (ACLRedis::getInstance()->hdel(key, publisher_ids) == -1)
        return 1;
    return 0;
}

int RedisHelper::getAllPublisher(const std::string &room_id, std::vector<Publisher> &publishers)
{
    std::string key = "publishers_" + room_id;
    std::vector<std::string> fields, values;
    if (ACLRedis::getInstance()->hvals(key, fields, values) == -1)
        return 1;
    publishers.clear();
    for (std::string &v : values)
    {
        Publisher p;
        if (!Publisher::fromJSON(v, p))
            publishers.push_back(p);
    }
    return 0;
}

int RedisHelper::addSubscriber(const std::string &room_id, const Subscriber &subscriber)
{
    std::string key = "subscribers_" + room_id;
    if (ACLRedis::getInstance()->hset(key, subscriber.id, subscriber.toJSON()) == -1)
        return 1;
    return 0;
}

int RedisHelper::removeSubscribers(const std::string &room_id, const std::vector<std::string> &subscriber_ids)
{
    std::string key = "subscribers_" + room_id;
    if (ACLRedis::getInstance()->hdel(key, subscriber_ids) == -1)
        return 1;
    return 0;
}

int RedisHelper::getAllSubscriber(const std::string &room_id, std::vector<Subscriber> &subscribers)
{
    std::string key = "subscribers_" + room_id;
    std::vector<std::string> fields, values;
    if (ACLRedis::getInstance()->hvals(key, fields, values) == -1)
        return 1;
    subscribers.clear();
    for (std::string &v : values)
    {
        Subscriber s;
        if (!Subscriber::fromJSON(v, s))
            subscribers.push_back(s);
    }
    return 0;
}

int RedisHelper::getAllErizoAgent(const std::string &area, std::vector<ErizoAgent> &agents)
{
    std::vector<std::string> fields, values;
    std::string key = "erizo_agent_" + area;
    if (ACLRedis::getInstance()->hvals(key, fields, values) == -1)
        return 1;
    agents.clear();
    for (std::string &v : values)
    {
        ErizoAgent a;
        if (!ErizoAgent::fromJSON(v, a))
            agents.push_back(a);
    }
    return 0;
}

int RedisHelper::addBridgeStream(const std::string &room_id, const BridgeStream &bridge_stream)
{
    std::string key = "bridge_stream_" + room_id;
    if (ACLRedis::getInstance()->hset(key, bridge_stream.id, bridge_stream.toJSON()) == -1)
        return 1;
    return 0;
}

int RedisHelper::getBridgeStream(const std::string &room_id, const std::string &bridge_stream_id, BridgeStream &bridge_stream)
{
    std::string key = "bridge_stream_" + room_id;
    std::string buf;
    if (ACLRedis::getInstance()->hget(key, bridge_stream_id, buf) == -1)
        return 1;
    if (BridgeStream::fromJSON(buf, bridge_stream))
        return 1;
    return 0;
}

int RedisHelper::getAllBridgeStream(const std::string &room_id, std::vector<BridgeStream> &bridge_streams)
{
    std::string key = "bridge_stream_" + room_id;
    std::vector<std::string> fields, values;
    if (ACLRedis::getInstance()->hvals(key, fields, values) == -1)
        return 1;
    bridge_streams.clear();
    for (std::string &v : values)
    {
        BridgeStream s;
        if (!BridgeStream::fromJSON(v, s))
            bridge_streams.push_back(s);
    }
    return 0;
}

int RedisHelper::removeBridgeStream(const std::string &room_id, const std::string &bridge_stream_id)
{
    std::string key = "bridge_stream_" + room_id;
    if (ACLRedis::getInstance()->hdel(key, bridge_stream_id) == -1)
        return 1;
    return 0;
}

int RedisHelper::addClientToEC(const std::string &erizo_controller_id, const Client &client)
{
    if (ACLRedis::getInstance()->hset(erizo_controller_id, client.id, client.toJSON()) == -1)
        return 1;
    return 0;
}

int RedisHelper::removeClientFromEC(const std::string &erizo_controller_id, const std::string &client_id)
{
    if (ACLRedis::getInstance()->hdel(erizo_controller_id, client_id) == -1)
        return 1;
    return 0;
}
int RedisHelper::getAllClientFromEC(const std::string &erizo_controller_id, std::vector<Client> &clients)
{
    std::vector<std::string> fields, values;
    if (ACLRedis::getInstance()->hvals(erizo_controller_id, fields, values) == -1)
        return 1;
    clients.clear();
    for (std::string &v : values)
    {
        Client c;
        if (!Client::fromJSON(v, c))
            clients.push_back(c);
    }
    return 0;
}

int RedisHelper::addHeartbeatData(const ErizoController::HEARTBEAT &heartbeat_data)
{
    if (ACLRedis::getInstance()->hset("erizo_controller_heartbeat", heartbeat_data.id, heartbeat_data.toJSON()) == -1)
        return 1;
    return 0;
}
int RedisHelper::removeHeartbeatData(const std::string &id)
{
    if (ACLRedis::getInstance()->hdel("erizo_controller_heartbeat", id) == -1)
        return 1;
    return 0;
}
int RedisHelper::getAllHeartbeatData(std::vector<ErizoController::HEARTBEAT> &heartbeats)
{
    std::vector<std::string> fields, values;
    if (ACLRedis::getInstance()->hvals("erizo_controller_heartbeat", fields, values) == -1)
        return 1;
    heartbeats.clear();
    for (std::string &v : values)
    {
        ErizoController::HEARTBEAT h;
        if (!ErizoController::HEARTBEAT::fromJSON(v, h))
            heartbeats.push_back(h);
    }
    return 0;
}