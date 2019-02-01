#ifndef SOCKET_IO_SERVER_H
#define SOCKET_IO_SERVER_H

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <map>
#include <condition_variable>
#include <queue>

#include <uWS/uWS.h>

#include "common/logger.h"

class SocketIOClientHandler;

class SocketIOServer
{
    DECLARE_LOGGER();

    struct SIOData
    {
        std::string client_id;
        std::string message;
    };

  public:
    SocketIOServer();
    ~SocketIOServer();

    int init();
    void close();

    void onMessage(const std::function<std::string(SocketIOClientHandler *hdl, const std::string &)> &on_message)
    {
        on_message_hdl_ = on_message;
    }
    void onClose(const std::function<void(SocketIOClientHandler *hdl)> &on_close)
    {
        on_close_hdl_ = on_close;
    }

    void sendEvent(const std::string &client_id, const std::string &msg);

  private:
    std::function<std::string(SocketIOClientHandler *hdl, const std::string &)> on_message_hdl_;
    std::function<void(SocketIOClientHandler *hdl)> on_close_hdl_;

    std::mutex clients_mux_;
    std::map<std::string, SocketIOClientHandler *> clients_;
    std::vector<std::thread *> threads_;
    std::condition_variable send_cond_;
    std::queue<SIOData> send_queue_;
    std::mutex send_mux_;

    std::unique_ptr<std::thread> send_thread_;
    std::atomic<bool> run_;
    bool init_;
};

#endif