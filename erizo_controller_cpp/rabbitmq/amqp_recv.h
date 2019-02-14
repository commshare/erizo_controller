#ifndef AMQP_RECV_H
#define AMQP_RECV_H

#include <string>
#include <thread>
#include <memory>
#include <atomic>
#include <functional>

#include "common/logger.h"

class AMQPCli;

class AMQPRecv
{
  DECLARE_LOGGER();

public:
  AMQPRecv();
  ~AMQPRecv();

  int init(const std::string &binding_key, const std::function<void(const std::string &msg)> &func);
  void close();
  const std::string &getReplyTo();

private:
  std::string reply_to_;
  std::unique_ptr<AMQPCli> amqp_cli_;
  std::unique_ptr<std::thread> recv_thread_;
  std::atomic<bool> run_;
  bool init_;
};

#endif
