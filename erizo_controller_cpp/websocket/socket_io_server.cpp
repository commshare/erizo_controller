#include "socket_io_server.h"

#include "common/config.h"

DEFINE_LOGGER(SocketIOServer, "SocketIOServer");

SocketIOServer::SocketIOServer() : run_(false),
                                   init_(false)
{
    on_message_hdl_ = [&](SocketIOClientHandler *hdl, const std::string &msg) {
        ELOG_WARN("receive message:%s,but message handler not set");
        return "disconnect";
    };

    on_close_hdl_ = [&](SocketIOClientHandler *hdl) {
        ELOG_WARN("close handler not set");
        return;
    };
}

SocketIOServer::~SocketIOServer()
{
}

int SocketIOServer::init()
{
    if (init_)
        return 0;

    run_ = true;
    threads_.resize(20);
    std::transform(threads_.begin(), threads_.end(), threads_.begin(), [&](std::thread *t) {
        return new std::thread([&]() {
            uWS::Hub hub;
            hub.onConnection([&](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
                SocketIOClientHandler *hdl = new SocketIOClientHandler(ws, std::ref(on_message_hdl_), std::ref(on_close_hdl_));
                std::string client_id = hdl->getClient().id;
                ws->setUserData(hdl);

                std::unique_lock<std::mutex> lock(mux_);
                clients_[client_id] = hdl;
                ELOG_INFO("client %s connect,num:%d", client_id, clients_.size());
            });

            hub.onMessage([&](uWS::WebSocket<uWS::SERVER> *ws, char *data, size_t len, uWS::OpCode op_codec) {
                if (op_codec == uWS::OpCode::TEXT)
                {
                    void *ptr = ws->getUserData();
                    if (ptr == nullptr)
                        return;
                    SocketIOClientHandler *hdl = reinterpret_cast<SocketIOClientHandler *>(ptr);
                    hdl->onMessage(std::string(data, len));
                }
            });

            hub.onDisconnection([&](uWS::WebSocket<uWS::SERVER> *ws, int code, char *data, size_t len) {
                void *ptr = ws->getUserData();
                if (ptr == nullptr)
                    return;

                SocketIOClientHandler *hdl = reinterpret_cast<SocketIOClientHandler *>(ptr);
                std::string client_id = hdl->getClient().id;
                hdl->onClose();

                std::unique_lock<std::mutex> lock(mux_);
                clients_.erase(client_id);
                delete hdl;
                ELOG_INFO("client %s disconnect,num:%d", client_id, clients_.size());
            });

            if (Config::getInstance()->ssl_)
            {
                uS::TLS::Context context = uS::TLS::createContext(Config::getInstance()->ssl_cert_,
                                                                  Config::getInstance()->ssl_key_,
                                                                  Config::getInstance()->ssl_passwd_);
                if (!hub.listen(Config::getInstance()->ssl_port_, context, uS::ListenOptions::REUSE_PORT))
                {
                    ELOG_ERROR("socket-io server(tls) listen to %d failed", Config::getInstance()->ssl_port_);
                    return;
                }
            }
            else
            {
                if (!hub.listen(Config::getInstance()->port_, nullptr, uS::ListenOptions::REUSE_PORT))
                {
                    ELOG_ERROR("socket-io server listen to %d failed", Config::getInstance()->ssl_port_);
                    return;
                }
            }
            while (run_)
                hub.run();
        });
    });
    init_ = true;
    return 0;
}

void SocketIOServer::close()
{
    if (!init_)
        return;

    run_ = false;
    std::for_each(threads_.begin(), threads_.end(), [](std::thread *t) {
        t->join();
    });

    for (auto it = clients_.begin(); it != clients_.end(); it++)
        delete it->second;
    clients_.clear();

    init_ = false;
}

void SocketIOServer::sendEvent(const std::string &client_id, const std::string &msg)
{
    std::unique_lock<std::mutex>(mux_);
    auto it = clients_.find(client_id);
    if (it != clients_.end())
        it->second->sendEvent(msg);
}