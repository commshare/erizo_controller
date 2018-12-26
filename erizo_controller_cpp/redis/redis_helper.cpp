#include "redis_helper.h"

#include <boost/asio/ip/address.hpp>

#include "common/config.h"

DEFINE_LOGGER(RedisHelper, "RedisHelper");

RedisHelper::RedisHelper() : redis_(nullptr), init_(false) {}

RedisHelper::~RedisHelper() {}

int RedisHelper::init()
{
    if (init_)
    {
        ELOG_WARN("RedisHelper duplicate initialize!!!");
        return 0;
    }

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
    result = redis_->command("AUTH", {Config::getInstance()->redis_password_});
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
    val = redis_->command("HSET", {"rooms", room.id, room.toJSON()});
    if (!val.isOk())
    {
        ELOG_ERROR("Redis execute HSET failed");
        return 1;
    }
    return 0;
}

int RedisHelper::getRoom(const std::string &room_id, Room &room)
{
    redisclient::RedisValue val;
    val = redis_->command("HGET", {"rooms", room.id});
    if (!val.isOk() || !val.isString())
    {
        ELOG_ERROR("Redis execute HSET failed");
        return 1;
    }

    if (Room::fromJSON(val.toString(), room))
    {
        ELOG_ERROR("Convert json to Room Object failed");
        return 1;
    }

    return 0;
}

int RedisHelper::getAllRoom(std::vector<Room> &rooms)
{
    redisclient::RedisValue val;
    val = redis_->command("HVALS", {"rooms"});
    if (!val.isOk() || !val.isArray())
    {
        ELOG_ERROR("Redis execute HVALS failed");
        return 1;
    }

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

int RedisHelper::addPublisher(const std::string &room_id, const Publisher &pubilsher)
{
    redisclient::RedisValue val;
    val = redis_->command("HSET", {room_id, pubilsher.id, pubilsher.toJSON()});
    if (!val.isOk())
    {
        ELOG_ERROR("Redis execute HSET failed");
        return 1;
    }
    return 0;
}

int RedisHelper::getPublisher(const std::string &room_id, const std::string &publisher_id, Publisher &publisher)
{
    redisclient::RedisValue val;
    val = redis_->command("HGET", {room_id, publisher_id});
    if (!val.isOk() || !val.isString())
    {
        ELOG_ERROR("Redis execute HSET failed");
        return 1;
    }

    if (Publisher::fromJSON(val.toString(), publisher))
    {
        ELOG_ERROR("Convert json to Publisher Object failed");
        return 1;
    }

    return 0;
}

int RedisHelper::getAllPublisher(const std::string &room_id, std::vector<Publisher> &publishers)
{
    redisclient::RedisValue val;
    val = redis_->command("HVALS", {room_id});
    if (!val.isOk())
    {
        ELOG_ERROR("Redis execute HSET failed");
        return 1;
    }
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

int RedisHelper::addSubscriber(const std::string &publisher_id, const Subscriber &subscriber) { return 0; }
int RedisHelper::removePublisher(const std::string &room_id, const std::string &publisher_id) { return 0; }
int RedisHelper::removeSubscriber(const std::string &publisher_id, const std::string &subscriber_id) { return 0; }
int RedisHelper::removeRoom(const std::string &room_id) { return 0; }
// rooms [room1,room2,room3,room4]
// room1 [publisher1,publisher1,publishers,publisher1]
// publisher1 [subscriber1,subcriber2,subscriber3]