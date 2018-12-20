#ifndef ERIZO_CONTROLLER_H
#define ERIZO_CONTROLLER_H

#include <thread>
#include <memory>
#include <atomic>

#include <json/json.h>

#include "websocket/server.hpp"
#include "redis/redis_helper.h"
#include "rabbitmq/amqp_rpc.h"
#include "rabbitmq/amqp_rpc_boardcast.h"

struct ErizoAgent
{
  std::string id;
  std::string ip;
  int timeout;
};

class ErizoController
{
  DECLARE_LOGGER();

public:
  ErizoController();
  ~ErizoController();

  int init();
  void close();

private:
  int handleEvent(std::string ip, uint16_t port, const std::string &msg, std::string &reply);
  std::string handleToken(const Json::Value &root);

  void getErizoAgents(const Json::Value &root);

  //unimplement method
  ErizoAgent allocateAgent();
  void getErizo(std::string room_id);

private:
  std::shared_ptr<RedisHelper> redis_;
  std::shared_ptr<WSServer<server_tls>> ws_tls_;
  std::shared_ptr<WSServer<server_plain>> ws_;
  std::shared_ptr<AMQPRPC> amqp_;
  std::shared_ptr<AMQPRPCBoardcast> amqp_boardcast_;

  std::atomic<bool> run_;
  std::unique_ptr<std::thread> keeplive_thread_;

  std::mutex agents_map_mux_;
  std::map<std::string, ErizoAgent> agents_map_;

  std::mutex clients_map_mux_;
  // std::map<std::string, Client> clients_map_;
};

#endif
