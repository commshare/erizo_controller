#ifndef ERIZO_CONTROLLER_H
#define ERIZO_CONTROLLER_H

#include <json/json.h>

#include "websocket/server.hpp"
#include "redis/redis_helper.h"

class ErizoController
{
  DECLARE_LOGGER();

public:
  ErizoController();
  ~ErizoController();

  int init();
  void close();

private:
  bool handleEvent(const std::string &msg, std::string &reply);
  std::string handleToken(const Json::Value &root);

  bool getErizoAgent(const std::string &client_ip, std::string &agent_id);

private:
  std::shared_ptr<RedisHelper> redis_;
  std::shared_ptr<WSServer<server_tls>> ws_tls_;
  std::shared_ptr<WSServer<server_plain>> ws_;
};

#endif