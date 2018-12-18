#ifndef WEBSOCKET_CLIENT_HANDLER_HPP
#define WEBSOCKET_CLIENT_HANDLER_HPP

#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <json/json.h>
#include <logger.h>

#include "common/utils.h"

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

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

template <typename T>
class ClientHandler
{
    DECLARE_LOGGER();

  public:
    ClientHandler(T *server,
                  websocketpp::connection_hdl hdl,
                  const std::function<void(T *, websocketpp::connection_hdl, const std::string &)> &callback = [](T *, websocketpp::connection_hdl, const std::string &) {});
    ~ClientHandler();

    void setAddress(const std::string &ip, uint16_t port)
    {
        ip_ = ip;
        port_ = port;
    }

    void getAddress(std::string &ip, uint16_t &port)
    {
        ip = ip_;
        port = port_;
    }

  private:
    void onMessage(T *s, websocketpp::connection_hdl hdl, typename T::message_ptr msg);
    void handleMessage(const std::string &payload);
    void handlePing();

  private:
    T *server_;
    websocketpp::connection_hdl hdl_;
    std::function<void(T *, websocketpp::connection_hdl, const std::string &)> callback_;
    std::string ip_;
    uint16_t port_;
};

template <typename T>
DEFINE_LOGGER(ClientHandler<T>, "ClientHandler");

template <typename T>
ClientHandler<T>::ClientHandler(T *server,
                                websocketpp::connection_hdl hdl,
                                const std::function<void(T *, websocketpp::connection_hdl, const std::string &)> &callback) : server_(server),
                                                                                                                              hdl_(hdl),
                                                                                                                              callback_(callback)
{

    typename T::connection_ptr conn = server_->get_con_from_hdl(hdl_);
    conn->set_message_handler(bind(&ClientHandler::onMessage, this, server_, ::_1, ::_2));

    Json::Value handshake;
    handshake["sid"] = Utils::getUUID();
    handshake["upgrades"] = Json::arrayValue;
    handshake["pingInterval"] = 50000;
    handshake["pingTimeout"] = 10000;

    Json::FastWriter writer;
    std::string msg = "0" + writer.write(handshake);
    server->send(hdl, msg, websocketpp::frame::opcode::TEXT);
    server->send(hdl, "40", websocketpp::frame::opcode::TEXT);
}

template <typename T>
ClientHandler<T>::~ClientHandler()
{
}

template <typename T>
void ClientHandler<T>::onMessage(T *s, websocketpp::connection_hdl hdl, typename T::message_ptr msg)
{
    std::string payload = msg->get_payload();
    ELOG_INFO("recevice message:%s", payload);

    SOCKET_IO_FRAME_TYPE frame_type = (SOCKET_IO_FRAME_TYPE)(payload[0] - '0');
    switch (frame_type)
    {
    case SOCKET_IO_FRAME_TYPE::frame_ping:
        handlePing();
        break;
    case SOCKET_IO_FRAME_TYPE::frame_message:
        handleMessage(payload);
        break;
    default:
        break;
    }
}

template <typename T>
void ClientHandler<T>::handlePing()
{
    server_->send(hdl_, "3", websocketpp::frame::opcode::TEXT);
}

template <typename T>
void ClientHandler<T>::handleMessage(const std::string &payload)
{
    SOCKET_IO_MSG_TYPE msg_type = (SOCKET_IO_MSG_TYPE)(payload[1] - '0');
    if (msg_type != SOCKET_IO_MSG_TYPE::type_event)
        return;
    callback_(server_, hdl_, payload);
}

#endif