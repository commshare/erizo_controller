#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>

#include <json/json.h>

#include "common/logger.h"

class Config
{
  DECLARE_LOGGER();

public:
  static Config *getInstance();
  ~Config();
  int init(const std::string &config_file);

public:
  unsigned short port;
  bool ssl;
  std::string ssl_key;
  std::string ssl_cert;
  std::string ssl_passwd;
  unsigned short ssl_port;

  std::string redis_ip;
  unsigned short redis_port;
  std::string redis_passwd;
  int redis_conn_timeout;
  int redis_rw_timeout;
  int redis_max_conns;
  int redis_lock_timeout;
  int redis_lock_try_time;

  std::string rabbitmq_username;
  std::string rabbitmq_passwd;
  std::string rabbitmq_hostname;
  unsigned short rabbitmq_port;
  int rabbitmq_timeout;
  std::string uniquecast_exchange;
  std::string boardcast_exchange;

  int socket_io_thread_num;
  int erizo_controller_worker_num;
  int erizo_agent_timeout;
  int erizo_controller_update_interval;
  int erizo_controller_timeout;
  std::map<int, std::string> server_mapping;

private:
  Config();

private:
  static Config *instance_;
};

#endif