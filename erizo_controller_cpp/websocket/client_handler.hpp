#ifndef WEBSOCKET_CLIENT_HANDLER_HPP
#define WEBSOCKET_CLIENT_HANDLER_HPP

#include <thread>
#include <mutex>
#include <memory>

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
                  const std::function<std::string(ClientHandler<T> *, const std::string &)> &on_message,
                  const std::function<void(ClientHandler<T> *)> &on_shutdown);
    ~ClientHandler();

    void sendEvent(const std::string &msg);

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

    void goingToShutdown()
    {
        on_shutdown_(this);
    }

  private:
    void onMessage(T *s, websocketpp::connection_hdl hdl, typename T::message_ptr msg);
    void handleMessage(const std::string &payload);
    void handlePing();

  private:
    T *server_;
    websocketpp::connection_hdl hdl_;
    std::function<std::string(ClientHandler<T> *, const std::string &)> on_message_;
    std::function<void(ClientHandler<T> *)> on_shutdown_;
    std::string ip_;
    uint16_t port_;

    std::string test;
};

template <typename T>
DEFINE_LOGGER(ClientHandler<T>, "ClientHandler");

template <typename T>
ClientHandler<T>::ClientHandler(T *server,
                                websocketpp::connection_hdl hdl,
                                const std::function<std::string(ClientHandler<T> *, const std::string &)> &on_message,
                                const std::function<void(ClientHandler<T> *)> &on_shutdown) : server_(server),
                                                                                              hdl_(hdl),
                                                                                              on_message_(on_message),
                                                                                              on_shutdown_(on_shutdown),
                                                                                              ip_(""),
                                                                                              port_(0) //,
                                                                                                       // client_id_("")
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

    int mid;
    bool has_mid = false;

    size_t pos = payload.find_first_of('[');
    if (pos == payload.npos)
        return;

    if (pos > 2)
    {
        has_mid = true;
        mid = std::stoi(payload.substr(2, pos));
    }

    std::string event = payload.substr(pos);
    std::string msg = on_message_(this, event);
    if (!msg.compare("shutdown"))
    {
        // if message is null,notify client disconnect
        server_->send(hdl_, "41", websocketpp::frame::opcode::TEXT);
        return;
    }
    else if (!msg.compare("notreply"))
    {
        // do nothing,just return
        return;
    }
    else
    {
        std::ostringstream oss;
        oss << "43";
        if (has_mid)
            oss << mid;
        oss << msg;
        server_->send(hdl_, oss.str(), websocketpp::frame::opcode::TEXT);
    }
}

template <typename T>
void ClientHandler<T>::sendEvent(const std::string &msg)
{
    std::ostringstream oss;
    oss << "42";
    oss << msg;
    server_->send(hdl_, oss.str(), websocketpp::frame::opcode::TEXT);
}

#endif