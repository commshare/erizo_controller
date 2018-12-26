#ifndef AMQP_RPC_BOARDCAST_H
#define AMQP_RPC_BOARDCAST_H

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <json/json.h>
#include <logger.h>

#include <thread>
#include <memory>
#include <atomic>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>

class AMQPRPCBoardcast
{
  DECLARE_LOGGER();

  struct AMQPData
  {
    std::string exchange;
    std::string queuename;
    std::string binding_key;
    std::string msg;
  };

public:
  AMQPRPCBoardcast();
  ~AMQPRPCBoardcast();

  int init(const std::function<void(const std::string &msg)> &func);
  void close();
  void addRPC(const std::string &queuename,
              const std::string &binding_key,
              const Json::Value &data);

private:
  int checkError(amqp_rpc_reply_t x);
  int callback(const std::string &exchange,
               const std::string &queuename,
               const std::string &binding_key,
               const std::string &send_msg);
  std::string stringifyBytes(amqp_bytes_t bytes);

private:
  bool init_;
  std::atomic<bool> run_;
  std::string reply_to_;

  amqp_connection_state_t conn_;
  std::unique_ptr<std::thread> recv_thread_;
  std::unique_ptr<std::thread> send_thread_;

  std::mutex mux_;
  std::condition_variable cond_;
  std::queue<AMQPData> queue_;
};

#endif