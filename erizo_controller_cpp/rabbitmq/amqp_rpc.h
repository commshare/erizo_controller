#ifndef RABBITMQ_RPC_H
#define RABBITMQ_RPC_H

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <json/json.h>

#include <string>
#include <thread>
#include <memory>
#include <atomic>
#include <vector>
#include <functional>
#include <queue>
#include <condition_variable>
#include <mutex>

#include "common/logger.h"

class AMQPCli;

class AMQPRPC
{
    DECLARE_LOGGER();

    struct AMQPData
    {
        std::string exchange;
        std::string queuename;
        std::string binding_key;
        std::string msg;
    };

    struct AMQPCallback
    {
        std::atomic<uint64_t> ts;
        std::function<void(const Json::Value &)> func;
        std::mutex mux;
        std::string dump;
        AMQPCallback()
        {
            ts = 0;
            func = std::function<void(const Json::Value &)>([](const Json::Value &) {});
        }
        AMQPCallback(const AMQPCallback &a) {}
    };

  public:
    AMQPRPC();
    ~AMQPRPC();

    int init(const std::string &binding_key);
    void close();

    void rpc(const std::string &exchange,
             const std::string &queuename,
             const std::string &binding_key,
             const Json::Value &data,
             const std::function<void(const Json::Value &)> &func);
    int rpc(const std::string &queuename, const Json::Value &data);
    void rpcNotReply(const std::string &queuename, const Json::Value &data);

  private:
    int send(const std::string &exchange,
             const std::string &queuename,
             const std::string &binding_key,
             const std::string &send_msg);

    void handleCallback(const std::string &msg);

  private:
    std::mutex send_queue_mux_;
    std::condition_variable send_cond_;
    std::queue<AMQPData> send_queue_;
    std::vector<AMQPCallback> cb_queue_;

    std::string reply_to_;
    std::atomic<uint32_t> index_;

    std::unique_ptr<AMQPCli> amqp_cli_;
    std::unique_ptr<std::thread> recv_thread_;
    std::unique_ptr<std::thread> send_thread_;
    std::unique_ptr<std::thread> check_thread_;
    std::atomic<bool> run_;
    bool init_;
};

#endif