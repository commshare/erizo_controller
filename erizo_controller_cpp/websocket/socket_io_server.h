#ifndef SOCKET_IO_SERVER_H
#define SOCKET_IO_SERVER_H

#include <string>
#include <sstream>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <memory>
#include <map>

#include <uWS/uWS.h>

#include "common/logger.h"
#include "socket_io_client_handler.h"

class ClientHdl
{
};

class SocketIOServer
{
    DECLARE_LOGGER();

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

    std::mutex mux_;
    std::map<std::string, SocketIOClientHandler *> clients_;

    std::vector<std::thread *> threads_;
    std::atomic<bool> run_;
    bool init_;
};

#endif