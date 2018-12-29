#ifndef AMQP_RECV_H
#define AMQP_RECV_H

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <json/json.h>

#include <thread>
#include <memory>
#include <atomic>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>

#include "common/logger.h"

class AMQPRecv
{
    DECLARE_LOGGER();

  public:
    AMQPRecv();
    ~AMQPRecv();

    int init(const std::function<void(const std::string &msg)> &func);
    void close();

    std::string getReplyTo() { return reply_to_; }

  private:
    int checkError(amqp_rpc_reply_t x);
    std::string stringifyBytes(amqp_bytes_t bytes);

  private:
    bool init_;
    std::atomic<bool> run_;
    std::string reply_to_;

    amqp_connection_state_t conn_;
    std::unique_ptr<std::thread> recv_thread_;
};

#endif
