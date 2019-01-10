#ifndef RABBITMQ_RPC_H
#define RABBITMQ_RPC_H

#include <string>
#include <thread>
#include <memory>
#include <atomic>
#include <vector>
#include <functional>
#include <queue>
#include <condition_variable>
#include <mutex>

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include <json/json.h>

#include "common/logger.h"

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
        Json::Value data;
        std::condition_variable cond;
        std::mutex mux;
        //******************DEBUG*****************
        std::string dump;
        //******************DEBUG*****************
        AMQPCallback()
        {
            ts = 0;
            data = Json::nullValue;
        }
        AMQPCallback(const AMQPCallback &a) {}
    };

  public:
    AMQPRPC();
    ~AMQPRPC();

    int init();
    void close();
    void addRPC(const std::string &exchange,
                const std::string &queuename,
                const std::string &binding_key,
                const Json::Value &data,
                const std::function<void(const Json::Value &)> &func);

    void sendMessage(const std::string &exchange,
                     const std::string &queuename,
                     const std::string &binding_key,
                     const Json::Value &data);

  private:
    int checkError(amqp_rpc_reply_t x);
    void handleCallback(const std::string &msg);
    int callback(const std::string &exchange,
                 const std::string &queuename,
                 const std::string &binding_key,
                 const std::string &send_msg);

    std::string stringifyBytes(amqp_bytes_t bytes);
    void notifyAllCallbackThread();

  private:
    bool init_;
    std::atomic<bool> run_;
    amqp_connection_state_t conn_;

    std::unique_ptr<std::thread> recv_thread_;
    std::unique_ptr<std::thread> send_thread_;
    std::unique_ptr<std::thread> check_thread_;

    std::string reply_to_;
    std::atomic<int> index_;

    std::mutex cb_queue_mux_;
    std::vector<AMQPCallback> cb_queue_;

    std::mutex send_queue_mux_;
    std::condition_variable send_cond_;
    std::queue<AMQPData> send_queue_;
};

#endif