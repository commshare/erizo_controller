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
  void onSignalingMessage(const std::string &msg);
  void addPublisher(const std::string &erizo_id,
                    const std::string &client_id,
                    const std::string &stream_id,
                    const std::string &label,
                    const std::function<void(const Json::Value &)> &func);
  void processSignaling(const std::string &erizo_id,
                        const std::string &client_id,
                        const std::string &stream_id,
                        const Json::Value &msg,
                        const std::function<void(const Json::Value &)> &func);
  void getErizo(const std::string &agent_id,
                const std::string &room_id,
                const std::function<void(const Json::Value &)> &func);
  void getErizoAgents(const Json::Value &root);
  template <typename T>
  int daptch(ClientHandler<T> * hdl, const std::string &msg, std::string &reply_msg);

  Json::Value handleToken(Client &client, const Json::Value &root);
  Json::Value handlePublish(Client &client, const Json::Value &root);
  void handleSignaling(Client &client, const Json::Value &root);

private:
  std::shared_ptr<RedisHelper> redis_;
  std::shared_ptr<WSServer<server_tls>> ws_tls_;
  std::shared_ptr<WSServer<server_plain>> ws_;
  std::shared_ptr<AMQPRPC> amqp_;
  std::shared_ptr<AMQPRPCBoardcast> amqp_boardcast_;
  std::shared_ptr<AMQPRecv> amqp_signaling_;

  std::atomic<bool> run_;
  std::unique_ptr<std::thread> keeplive_thread_;

  std::mutex agents_map_mux_;
  std::map<std::string, ErizoAgent> agents_map_;

  std::mutex clients_map_mux_;
  std::map<void *, Client> clients_map_;

  std::mutex streams_map_mux_;
  std::map<std::string, Stream> streams_map_;
};

#endif
