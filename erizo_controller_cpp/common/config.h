#ifndef CONFIG_H
#define CONFIG_H

#include <string>

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
  // Websocket config
  unsigned short port_;
  bool ssl_;
  std::string ssl_key_;
  std::string ssl_cert_;
  std::string ssl_passwd_;
  unsigned short ssl_port_;

  // Mysql config
  std::string mysql_url_;
  std::string mysql_username_;
  std::string mysql_password_;

  // Redis config
  std::string redis_ip_;
  unsigned short redis_port_;
  std::string redis_password_;

  //Rabbitmq config;
  std::string rabbitmq_username_;
  std::string rabbitmq_passwd_;
  std::string rabbitmq_hostname_;
  unsigned short rabbitmq_port_;
  int rabbitmq_timeout_;
  std::string uniquecast_exchange_;
  std::string boardcast_exchange_;

  int thread_num_;
  int worker_num_;
  std::string default_area_;
  int erizo_agent_timeout_;

private:
  Config();

private:
  static Config *instance_;
};

#endif