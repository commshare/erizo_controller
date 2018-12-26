#ifndef ERIZO_CONTROLLER_H
#define ERIZO_CONTROLLER_H

#include <thread>
#include <memory>
#include <atomic>
#include <functional>

#include <json/json.h>

#include "websocket/server.hpp"
#include "redis/redis_helper.h"
#include "rabbitmq/amqp_rpc.h"
#include "rabbitmq/amqp_rpc_boardcast.h"
#include "rabbitmq/amqp_recv.h"
#include "model/client.h"

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
  int addPublisher(const std::string &erizo_id,
                   const std::string &client_id,
                   const std::string &stream_id,
                   const std::string &label);
  int getErizo(const std::string &agent_id,
               const std::string &room_id,
               std::string &erizo_id);
  int processSignaling(const std::string &erizo_id,
                       const std::string &client_id,
                       const std::string &stream_id,
                       const Json::Value &msg);
  void onSignalingMessage(const std::string &msg);




  template <typename T>
  int daptch(ClientHandler<T> *hdl, const std::string &msg, std::string &reply_msg);
  Json::Value handleToken(Client &client, const Json::Value &root);
  Json::Value handlePublish(Client &client, const Json::Value &root);
  Json::Value handleSignaling(Client &client, const Json::Value &root);

private:
  std::shared_ptr<RedisHelper> redis_;
  std::shared_ptr<WSServer<server_tls>> ws_tls_;
  std::shared_ptr<WSServer<server_plain>> ws_;
  std::shared_ptr<AMQPRPC> amqp_;
  std::shared_ptr<AMQPRecv> amqp_signaling_;

};

#endif
