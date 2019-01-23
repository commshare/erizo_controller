#include "redis_helper.h"

int RedisHelper::addClient(const std::string &room_id, const Client &client)
{
    RedisLocker locker;
    if (!locker.lock(room_id))
        return 1;
    std::string key = "clients_" + room_id;
    if (ACLRedis::getInstance()->hset(key, client.id, client.toJSON()) == -1)
        return 1;
    locker.unlock();
    return 0;
}

int RedisHelper::removeClient(const std::string &room_id, const std::string &client_id)
{
    RedisLocker locker;
    if (!locker.lock(room_id))
        return 1;
    std::string key = "clients_" + room_id;
    if (ACLRedis::getInstance()->hdel(key, client_id) == -1)
        return 1;
    locker.unlock();
    return 0;
}

int RedisHelper::getAllClient(const std::string &room_id, std::vector<Client> &clients)
{
    RedisLocker locker;
    if (!locker.lock(room_id))
        return 1;
    std::string key = "clients_" + room_id;

    std::vector<std::string> fields, values;
    if (ACLRedis::getInstance()->hvals(key, fields, values) == -1)
        return 1;

    locker.unlock();

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
    RedisLocker locker;
    if (!locker.lock(room_id))
        return 1;
    std::string key = "publishers_" + room_id;
    if (ACLRedis::getInstance()->hset(key, pubilsher.id, pubilsher.toJSON()) == -1)
        return 1;
    locker.unlock();
    return 0;
}

int RedisHelper::setPublisherSSRC(const std::string &room_id, const std::string &publisher_id, uint32_t video_ssrc, uint32_t audio_ssrc)
{
    RedisLocker locker;
    if (!locker.lock(room_id))
        return 1;

    std::string key = "publishers_" + room_id;
    std::string buf;
    if (ACLRedis::getInstance()->hget(key, publisher_id, buf) == -1)
        return 1;
    Publisher publisher;
    if (Publisher::fromJSON(buf, publisher))
        return 1;
    publisher.video_ssrc = video_ssrc;
    publisher.audio_ssrc = audio_ssrc;

    if (ACLRedis::getInstance()->hset(key, publisher_id, publisher.toJSON()) == -1)
        return 1;
    locker.unlock();
    return 0;
}

int RedisHelper::getPublisher(const std::string &room_id, const std::string &publisher_id, Publisher &publisher)
{
    RedisLocker locker;
    if (!locker.lock(room_id))
        return 1;
    std::string key = "publishers_" + room_id;
    std::string buf;
    if (ACLRedis::getInstance()->hget(key, publisher_id, buf) == -1)
        return 1;
    locker.unlock();

    if (Publisher::fromJSON(buf, publisher))
        return 1;

    return 0;
}

int RedisHelper::removePublishers(const std::string &room_id, const std::vector<std::string> &publisher_ids)
{
    RedisLocker locker;
    if (!locker.lock(room_id))
        return 1;
    std::string key = "publishers_" + room_id;
    if (ACLRedis::getInstance()->hdel(key, publisher_ids) == -1)
        return 1;
    locker.unlock();
    return 0;
}

int RedisHelper::getAllPublisher(const std::string &room_id, std::vector<Publisher> &publishers)
{
    RedisLocker locker;
    if (!locker.lock(room_id))
        return 1;
    std::string key = "publishers_" + room_id;
    std::vector<std::string> fields, values;
    if (ACLRedis::getInstance()->hvals(key, fields, values) == -1)
        return 1;
    locker.unlock();

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
    RedisLocker locker;
    if (!locker.lock(room_id))
        return 1;
    std::string key = "subscribers_" + room_id;
    if (ACLRedis::getInstance()->hset(key, subscriber.id, subscriber.toJSON()) == -1)
        return 1;
    locker.unlock();
    return 0;
}

int RedisHelper::removeSubscribers(const std::string &room_id, const std::vector<std::string> &subscriber_ids)
{
    RedisLocker locker;
    if (!locker.lock(room_id))
        return 1;
    std::string key = "subscribers_" + room_id;
    if (ACLRedis::getInstance()->hdel(key, subscriber_ids) == -1)
        return 1;
    locker.unlock();
    return 0;
}

int RedisHelper::getAllSubscriber(const std::string &room_id, std::vector<Subscriber> &subscribers)
{
    RedisLocker locker;
    if (!locker.lock(room_id))
        return 1;
    std::string key = "subscribers_" + room_id;
    std::vector<std::string> fields, values;
    if (ACLRedis::getInstance()->hvals(key, fields, values) == -1)
        return 1;
    locker.unlock();

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
    if (ACLRedis::getInstance()->hvals(area, fields, values) == -1)
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
    RedisLocker locker;
    if (!locker.lock(room_id))
        return 1;
    std::string key = "bridge_stream_" + room_id;
    if (ACLRedis::getInstance()->hset(key, bridge_stream.id, bridge_stream.toJSON()) == -1)
        return 1;
    locker.unlock();
    return 0;
}

int RedisHelper::getBridgeStream(const std::string &room_id, const std::string &bridge_stream_id, BridgeStream &bridge_stream)
{
    RedisLocker locker;
    if (!locker.lock(room_id))
        return 1;
    std::string key = "bridge_stream_" + room_id;
    std::string buf;
    if (ACLRedis::getInstance()->hget(key, bridge_stream_id, buf) == -1)
        return 1;
    locker.unlock();

    if (BridgeStream::fromJSON(buf, bridge_stream))
        return 1;
    return 0;
}

int RedisHelper::getAllBridgeStream(const std::string &room_id, std::vector<BridgeStream> &bridge_streams)
{
    RedisLocker locker;
    if (!locker.lock(room_id))
        return 1;
    std::string key = "bridge_stream_" + room_id;
    std::vector<std::string> fields, values;
    if (ACLRedis::getInstance()->hvals(key, fields, values) == -1)
        return 1;
    locker.unlock();

    bridge_streams.clear();
    for (std::string &v : values)
    {
        BridgeStream s;
        if (!BridgeStream::fromJSON(v, s))
            bridge_streams.push_back(s);
    }
    return 0;
}
