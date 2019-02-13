#ifndef SOCKET_IO_CLIENT_HANDLER_H
#define SOCKET_IO_CLIENT_HANDLER_H

#include <string>
#include <functional>
#include <mutex>

#include <uWS/uWS.h>
#include <json/json.h>

#include "model/client.h"
#include "common/logger.h"

class SocketIOClientHandler
{
    DECLARE_LOGGER();

    enum SOCKET_IO_FRAME_TYPE
    {
        frame_open = 0,
        frame_close = 1,
        frame_ping = 2,
        frame_pong = 3,
        frame_message = 4,
        frame_upgrade = 5,
        frame_noop = 6
    };

    enum SOCKET_IO_MSG_TYPE
    {
        type_connect = 0,
        type_disconnect = 1,
        type_event = 2,
        type_ack = 3,
        type_error = 4,
        type_binary_event = 5,
        type_binary_ack = 6,
        type_undetermined = 0x10
    };

  public:
    SocketIOClientHandler(uWS::WebSocket<uWS::SERVER> *ws,
                          const std::function<std::string(SocketIOClientHandler *hdl, const std::string &)> &on_message,
                          const std::function<void(SocketIOClientHandler *hdl)> &on_close);
    ~SocketIOClientHandler();
    void onMessage(const std::string &msg);
    void onClose();

    void handleMessage(const std::string &msg);
    void sendMessage(const std::string &msg);

    Client &getClient()
    {
        return client_;
    }
    void setWebSocket(uWS::WebSocket<uWS::SERVER> *ws)
    {
        std::unique_lock<std::mutex>(mux_);
        ws_ = ws;
    }

  private:
    Client client_;
    uWS::WebSocket<uWS::SERVER> *ws_;
    std::function<std::string(SocketIOClientHandler *hdl, const std::string &)> on_message_hdl_;
    std::function<void(SocketIOClientHandler *hdl)> on_close_hdl_;
    std::mutex mux_;
};

#endif