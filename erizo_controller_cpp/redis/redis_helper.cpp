#include "redis_helper.h"

#include <boost/asio/ip/address.hpp>

#include "common/config.h"

DEFINE_LOGGER(RedisHelper, "RedisHelper");

RedisHelper::RedisHelper() : redis_(nullptr), init_(false) {}

RedisHelper::~RedisHelper() {}

redisclient::RedisValue RedisHelper::command(const std::string cmd, const std ::deque<redisclient::RedisBuffer> &buffer)
{
    std::unique_lock<std::mutex> lock(mux_);
    return redis_->command(cmd, buffer);
}

int RedisHelper::init()
{
    if (init_)
        return 0;

    boost::asio::ip::address address = boost::asio::ip::address::from_string(Config::getInstance()->redis_ip_);
    boost::asio::ip::tcp::endpoint endpoint(address, Config::getInstance()->redis_port_);
    boost::system::error_code ec;

    redis_ = std::make_shared<redisclient::RedisSyncClient>(ios_);
    redis_->connect(endpoint, ec);
    if (ec)
    {
        ELOG_ERROR("Connect to redis failed:%s", ec.message());
        return 1;
    }

    redisclient::RedisValue result;
    result = command("AUTH", {Config::getInstance()->redis_password_});
    if (result.isError())
    {
        ELOG_ERROR("Auth failed");
        return 1;
    }
    init_ = true;

    return 0;
}

void RedisHelper::close()
{
    if (!init_)
        return;
    redis_->disconnect();
    redis_.reset();
    redis_ = nullptr;
    init_ = false;
}

int RedisHelper::addClient(const std::string &room_id, const Client &client)
{
    redisclient::RedisValue val;
    std::string key = "clients_" + room_id;
    val = command("HSET", {key, client.id, client.toJSON()});
    if (!val.isOk())
        return 1;
    return 0;
}

int RedisHelper::removeClient(const std::string &room_id, const std::string &client_id)
{
    redisclient::RedisValue val;
    std::string key = "clients_" + room_id;
    val = command("HDEL", {key, client_id});
    if (!val.isOk())
        return 1;
    return 0;
}

int RedisHelper::getAllClient(const std::string &room_id, std::vector<Client> &clients)
{
    redisclient::RedisValue val;
    std::string key = "clients_" + room_id;
    val = command("HVALS", {key});
    if (!val.isOk() || !val.isArray())
        return 1;
    clients.clear();
    std::vector<redisclient::RedisValue> vec = val.toArray();
    for (redisclient::RedisValue v : vec)
    {
        if (v.isString())
        {
            Client c;
            if (!Client::fromJSON(v.toString(), c))
                clients.push_back(c);
        }
    }
    return 0;
}

int RedisHelper::addClientRoomMapping(const std::string &client_id, const std::string &room_id)
{
    redisclient::RedisValue val;
    val = command("HSET", {"clients", client_id, room_id});
    if (!val.isOk())
        return 1;
    return 0;
}

bool RedisHelper::isClientExist(const std::string &client_id)
{
    redisclient::RedisValue val;
    val = command("HGET", {"clients", client_id});
    if (!val.isOk() || !val.isString())
        return false;
    return true;
}

int RedisHelper::removeClientRoomMapping(const std::string &client_id)
{
    redisclient::RedisValue val;
    val = command("HDEL", {"clients", client_id});
    if (!val.isOk())
        return 1;
    return 0;
}

int RedisHelper::getRoomByClientId(const std::string &client_id, std::string &room_id)
{
    redisclient::RedisValue val;
    val = command("HGET", {"clients", client_id});
    if (!val.isOk() || !val.isString())
        return 1;
    room_id = val.toString();
    return 0;
}

int RedisHelper::addPublisher(const std::string &room_id, const Publisher &pubilsher)
{
    redisclient::RedisValue val;
    std::string key = "publishers_" + room_id;
    val = command("HSET", {key, pubilsher.id, pubilsher.toJSON()});
    if (!val.isOk())
        return 1;
    return 0;
}

int RedisHelper::getPublisher(const std::string &room_id, const std::string &publisher_id, Publisher &publisher)
{
    redisclient::RedisValue val;
    std::string key = "publishers_" + room_id;
    val = command("HGET", {key, publisher_id});
    if (!val.isOk() || !val.isString())
        return 1;
    if (Publisher::fromJSON(val.toString(), publisher))
        return 1;
    return 0;
}

int RedisHelper::removePublishers(const std::string &room_id, const std::vector<std::string> &publishers)
{
    std ::deque<redisclient::RedisBuffer> buffer;
    redisclient::RedisValue val;
    std::string key = "publishers_" + room_id;

    buffer.push_back(key);
    for (const std::string &publisher_id : publishers)
        buffer.push_back(publisher_id);

    val = command("HDEL", buffer);
    if (!val.isOk())
        return 1;
    return 0;
}

int RedisHelper::getAllPublisher(const std::string &room_id, std::vector<Publisher> &publishers)
{
    redisclient::RedisValue val;
    std::string key = "publishers_" + room_id;
    val = command("HVALS", {key});
    if (!val.isOk())
        return 1;
    publishers.clear();
    std::vector<redisclient::RedisValue> vec = val.toArray();
    for (redisclient::RedisValue v : vec)
    {
        if (v.isString())
        {
            Publisher p;
            if (!Publisher::fromJSON(v.toString(), p))
                publishers.push_back(p);
        }
    }
    return 0;
}

int RedisHelper::addSubscriber(const std::string &room_id, const Subscriber &subscriber)
{
    redisclient::RedisValue val;
    std::string key = "subscribers_" + room_id;
    val = command("HSET", {key, subscriber.id, subscriber.toJSON()});
    if (!val.isOk())
        return 1;
    return 0;
}

int RedisHelper::removeSubscribers(const std::string &room_id, const std::vector<std::string> &subscribers)
{
    std ::deque<redisclient::RedisBuffer> buffer;
    redisclient::RedisValue val;
    std::string key = "subscribers_" + room_id;

    buffer.push_back(key);
    for (const std::string &subscriber_id : subscribers)
        buffer.push_back(subscriber_id);
    val = command("HDEL", buffer);
    if (!val.isOk())
        return 1;
    return 0;
}

int RedisHelper::getAllSubscriber(const std::string &room_id, std::vector<Subscriber> &subscribers)
{
    redisclient::RedisValue val;
    std::string key = "subscribers_" + room_id;
    val = command("HVALS", {key});
    if (!val.isOk())
        return 1;

    subscribers.clear();
    std::vector<redisclient::RedisValue> vec = val.toArray();
    for (redisclient::RedisValue v : vec)
    {
        if (v.isString())
        {
            Subscriber s;
            if (!Subscriber::fromJSON(v.toString(), s))
                subscribers.push_back(s);
        }
    }
    return 0;
}

int RedisHelper::getAllErizoAgent(const std::string &area, std::vector<ErizoAgent> &agents)
{
    redisclient::RedisValue val;
    val = command("HVALS", {area});
    if (!val.isOk())
        return 1;

    agents.clear();
    std::vector<redisclient::RedisValue> vec = val.toArray();
    for (redisclient::RedisValue v : vec)
    {
        if (v.isString())
        {
            ErizoAgent a;
            if (!ErizoAgent::fromJSON(v.toString(), a))
                agents.push_back(a);
        }
    }
    return 0;
}

int RedisHelper::addBridgeStream(const std::string &room_id, const BridgeStream &bridge_stream)
{
    redisclient::RedisValue val;
    std::string key = "bridge_stream_" + room_id;
    val = command("HSET", {key, bridge_stream.id, bridge_stream.toJSON()});
    if (!val.isOk())
        return 1;
    return 0;
}

int RedisHelper::getBridgeStream(const std::string &room_id, const std::string &bridge_stream_id, BridgeStream &bridge_stream)
{
    redisclient::RedisValue val;
    std::string key = "bridge_stream_" + room_id;
    val = command("HGET", {key, bridge_stream_id});
    if (!val.isOk() || !val.isString())
        return 1;
    if (BridgeStream::fromJSON(val.toString(), bridge_stream))
        return 1;
    return 0;
}

int RedisHelper::getAllBridgeStream(const std::string &room_id, std::vector<BridgeStream> &bridge_streams)
{
    redisclient::RedisValue val;
    std::string key = "bridge_stream_" + room_id;
    val = command("HVALS", {key});
    if (!val.isOk())
        return 1;

    bridge_streams.clear();
    std::vector<redisclient::RedisValue> vec = val.toArray();
    for (redisclient::RedisValue v : vec)
    {
        if (v.isString())
        {
            BridgeStream s;
            if (!BridgeStream::fromJSON(v.toString(), s))
                bridge_streams.push_back(s);
        }
    }
    return 0;
}

int RedisHelper::addStream(const std::string &client_id, const Stream &stream)
{
    redisclient::RedisValue val;

    if (!isClientExist(client_id))
        return 1;

    val = command("HSET", {"stream", stream.id, stream.toJSON()});
    if (!val.isOk())
        return 1;
    return 0;
}

int RedisHelper::getStream(const std::string &stream_id, Stream &stream)
{
    redisclient::RedisValue val;
    val = command("HGET", {"stream", stream_id});
    if (!val.isOk() || !val.isString())
        return 1;
    if (Stream::fromJSON(val.toString(), stream))
        return 1;
    return 0;
}
