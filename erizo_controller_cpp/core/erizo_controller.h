#ifndef ERIZO_CONTROLLER_H
#define ERIZO_CONTROLLER_H

#include <thread>
#include <memory>
#include <atomic>
#include <functional>

#include <json/json.h>

#include "thread/thread_pool.h"
#include "common/logger.h"
#include "model/client.h"
#include "model/bridge_stream.h"

class AMQPRPC;
class AMQPRecv;
class SocketIOServer;
class SocketIOClientHandler;

namespace erizo
{
class ThreadPool;
}

class ErizoController
{
  DECLARE_LOGGER();

public:
  ~ErizoController();
  static ErizoController *getInstance();

  int init();
  void close();

private:
  ErizoController();

  void notifyToSubscribe(const std::string &room_id,
                         const std::string &client_id,
                         const std::string &stream_id);

  void asyncTask(const std::function<void()> &func);

  int allocAgent(Client &client);

  int allocErizo(Client &client);

  void addPublisher(const Client &client, const Publisher &publisher);
  void removePublisher(const Publisher &publisher);

  void addVirtualPublisher(const Publisher &publisher, const BridgeStream &bridge_stream);
  void removeVirtualPublisher(const BridgeStream &bridge_stream);

  void addSubscriber(const Publisher &publisher, const Subscriber &subscriber);
  void removeSubscriber(const Subscriber &subscriber);

  void notifyToRemoveSubscriber(const Subscriber &subscriber);

  void addVirtualSubscriber(const BridgeStream &bridge_stream);
  void removeVirtualSubscriber(const BridgeStream &bridge_stream);

  void processSignaling(const std::string &erizo_id,
                        const std::string &client_id,
                        const std::string &stream_id,
                        const Json::Value &msg);

  void onSignalingMessage(const std::string &msg);

  void onBoardcastMessage(const std::string &msg);

  std::string onMessage(SocketIOClientHandler *hdl, const std::string &msg);

  void onClose(SocketIOClientHandler *hdl);

  Json::Value handleToken(Client &client, const Json::Value &root);

  Json::Value handlePublish(Client &client, const Json::Value &root);

  Json::Value handleSubscribe(Client &client, const Json::Value &root);

  void handleSignaling(Client &client, const Json::Value &root);

  int removeBridgeStreamPub(const std::string &room_id, const std::string &stream_id);
  int removeBridgeStreamSub(const std::string &room_id, const std::string &subscribe_to);

private:
  std::shared_ptr<SocketIOServer> socket_io_;
  std::shared_ptr<AMQPRPC> amqp_;
  std::shared_ptr<AMQPRecv> amqp_signaling_;
  std::shared_ptr<AMQPRecv> amqp_boardcast_; 
  std::unique_ptr<erizo::ThreadPool> thread_pool_;
  bool init_;

  static ErizoController *instance_;
};

#endif
