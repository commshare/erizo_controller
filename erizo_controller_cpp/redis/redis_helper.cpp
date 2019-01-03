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

int RedisHelper::addRoom(const Room &room)
{
    redisclient::RedisValue val;
    val = command("HSET", {"rooms", room.id, room.toJSON()});
    if (!val.isOk())
        return 1;
    return 0;
}

int RedisHelper::getRoom(const std::string &room_id, Room &room)
{
    redisclient::RedisValue val;
    val = command("HGET", {"rooms", room_id});
    if (!val.isOk() || !val.isString())
        return 1;
    if (Room::fromJSON(val.toString(), room))
        return 1;
    return 0;
}

int RedisHelper::getAllRoom(std::vector<Room> &rooms)
{
    redisclient::RedisValue val;
    val = command("HVALS", {"rooms"});
    if (!val.isOk() || !val.isArray())
        return 1;

    rooms.clear();
    std::vector<redisclient::RedisValue> vec = val.toArray();
    for (redisclient::RedisValue v : vec)
    {
        if (v.isString())
        {
            Room r;
            if (!Room::fromJSON(v.toString(), r))
                rooms.push_back(r);
        }
    }
    return 0;
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

int RedisHelper::removePublisher(const std::string &room_id, const std::string &publisher_id) { return 0; }
int RedisHelper::removeSubscriber(const std::string &publisher_id, const std::string &subscriber_id) { return 0; }
int RedisHelper::removeRoom(const std::string &room_id) { return 0; }
// rooms [room1,room2,room3,room4]
// room1 [publisher1,publisher1,publishers,publisher1]
// publisher1 [subscriber1,subcriber2,subscriber3]