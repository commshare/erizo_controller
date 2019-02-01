#include "redis_locker.h"

#include <sstream>

#include "acl_redis.h"
#include "common/utils.h"
#include "common/config.h"

RedisLocker::RedisLocker() : key_(""),
                             locked_(false)

{
}

RedisLocker::~RedisLocker()
{
    unlock();
}

bool RedisLocker::try_lock(const std::string &key)
{
    if (locked_)
        return 1;

    key_ = key;
    std::ostringstream oss;
    uint64_t now = Utils::getSystemMs();
    oss << now;

    if (!ACLRedis::getInstance()->setnx(key, oss.str()))
    {
        std::string value;
        if (!ACLRedis::getInstance()->get(key, value))
            return 1;

        if (value.empty())
        {
            //nil
            if (!ACLRedis::getInstance()->getset(key, oss.str(), value))
                return 1;
            if (value.empty())
                locked_ = true;
            else
                locked_ = false;
        }
        else
        {
            uint64_t t1 = std::stoll(value);
            if (now > t1 + Config::getInstance()->redis_lock_timeout)
            {

                if (!ACLRedis::getInstance()->getset(key, oss.str(), value))
                    return 1;

                uint64_t t2 = std::stoll(value);
                if (t2 == t1)
                    locked_ = true;
                else
                    locked_ = false;
            }
        }
    }
    else
    {
        locked_ = true;
    }

    return locked_;
}

void RedisLocker::unlock()
{
    if (!locked_)
        return;
    ACLRedis::getInstance()->del(key_);
    locked_ = false;
}

bool RedisLocker::lock(const std::string &key)
{
    bool ret;
    int try_time = Config::getInstance()->redis_lock_try_time;
    do
    {
        ret = try_lock(key);
        if (!ret)
            usleep(10000); // 10ms
    } while (!ret && try_time--);
    return ret;
}