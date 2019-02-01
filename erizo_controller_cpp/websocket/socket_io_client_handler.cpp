#include "socket_io_client_handler.h"

#include "route/route.h"
#include "common/utils.h"

DEFINE_LOGGER(SocketIOClientHandler, "SocketIOClientHandler");

SocketIOClientHandler::SocketIOClientHandler(uWS::WebSocket<uWS::SERVER> *ws,
                                             const std::function<std::string(SocketIOClientHandler *hdl, const std::string &)> &on_message,
                                             const std::function<void(SocketIOClientHandler *hdl)> &on_close) : ws_(ws),
                                                                                                                on_message_hdl_(on_message),
                                                                                                                on_close_hdl_(on_close)

{
    uS::Socket::Address addr = ws->getAddress();
    client_.id = "cli_" + Utils::getUUID();
    client_.ip = addr.address;
    client_.port = addr.port;
    client_.family = addr.family;

    if (client_.family == "IPv4")
    {
        client_.ip_info = Route::getInstance()->processIP(client_.ip);
    }
    else
    {
        std::string ipv4_addr;
        if (!Utils::searchAddress(addr.address, ipv4_addr))
            client_.ip_info = Route::getInstance()->processIP(ipv4_addr);
    }

    Json::Value handshake;
    handshake["sid"] = Utils::getUUID();
    handshake["upgrades"] = Json::arrayValue;
    handshake["pingInterval"] = 50000;
    handshake["pingTimeout"] = 10000;

    Json::FastWriter writer;
    std::string msg = "0" + writer.write(handshake);

    sendMessage(msg);
    sendMessage("40");
}

SocketIOClientHandler::~SocketIOClientHandler()
{
}

void SocketIOClientHandler::handleMessage(const std::string &msg)
{

    SOCKET_IO_MSG_TYPE msg_type = (SOCKET_IO_MSG_TYPE)(msg[1] - '0');
    if (msg_type != SOCKET_IO_MSG_TYPE::type_event)
        return;

    int mid;
    bool has_mid = false;

    size_t pos = msg.find_first_of('[');
    if (pos == msg.npos)
        return;

    if (pos > 2)
    {
        has_mid = true;
        mid = std::stoi(msg.substr(2, pos));
    }

    std::string event = msg.substr(pos);
    std::string res = on_message_hdl_(this, event);
    if (res == "disconnect")
    {
        sendMessage("41");
    }
    else if (res == "keep")
    {
    }
    else
    {
        std::ostringstream oss;
        oss << "43";
        if (has_mid)
            oss << mid;
        oss << res;

        sendMessage(oss.str());
    }
}

void SocketIOClientHandler::onMessage(const std::string &msg)
{
    SOCKET_IO_FRAME_TYPE frame_type = (SOCKET_IO_FRAME_TYPE)(msg[0] - '0');
    switch (frame_type)
    {
    case SOCKET_IO_FRAME_TYPE::frame_ping:
        sendMessage("3");
        break;
    case SOCKET_IO_FRAME_TYPE::frame_message:
        handleMessage(msg);
        break;
    default:
        break;
    }
}

void SocketIOClientHandler::onClose()
{
    on_close_hdl_(this);
}

void SocketIOClientHandler::sendMessage(const std::string &msg)
{
    std::unique_lock<std::mutex>(mux_);
    if (ws_ != nullptr)
        ws_->send(msg.c_str(), msg.length(), uWS::OpCode::TEXT);
}

void SocketIOClientHandler::sendEvent(const std::string &msg)
{
    std::ostringstream oss;
    oss << "42";
    oss << msg;
    sendMessage(oss.str());
}