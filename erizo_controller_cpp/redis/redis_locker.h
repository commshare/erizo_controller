#ifndef REDIS_LOCKER_H
#define REDIS_LOCKER_H

#include <string>

#include "acl_redis.h"

class RedisLocker
{
public:
  RedisLocker();
  ~RedisLocker();
  bool lock(const std::string &key);
  void unlock();

private:
  bool try_lock(const std::string &key);

private:
  std::string key_;
  bool locked_;
};

#endif