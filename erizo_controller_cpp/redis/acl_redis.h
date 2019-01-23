#ifndef ACL_REDIS_H
#define ACL_REDIS_H

#include <vector>
#include <thread>
#include <atomic>

#include <acl_cpp/lib_acl.hpp>

class ACLRedis
{
public:
  static ACLRedis *getInstance();
  ~ACLRedis();

  int init();
  void close();

  int setnx(const std::string &key, const std::string &value);
  int get(const std::string &key, std::string &value);
  int getset(const std::string &key, const std::string &value, std::string &old_value);
  int del(const std::string &key);
  int set(const std::string &key, const std::string &value);
  int hset(const std::string &key, const std::string &field, const std::string &value);
  int hget(const std::string &key, const std::string &field, std::string &value);
  int hvals(const std::string &key, std::vector<std::string> &fields, std::vector<std::string> &values);
  int hdel(const std::string &key, const std::string &field);
  int hdel(const std::string &key, const std::vector<std::string> &fields);

private:
  ACLRedis();

private:
  std::shared_ptr<acl::redis_client_cluster> cluster_;
  bool init_;
  static ACLRedis *instance_;
};

#endif