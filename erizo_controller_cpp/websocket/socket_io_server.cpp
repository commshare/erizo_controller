#include "socket_io_server.h"

#include <sstream>

#include "socket_io_client_handler.h"
#include "common/config.h"

DEFINE_LOGGER(SocketIOServer, "SocketIOServer");

SocketIOServer::SocketIOServer() : send_thread_(nullptr),
                                   run_(false),
                                   init_(false)
{
    on_message_hdl_ = [this](SocketIOClientHandler *hdl, const std::string &msg) {
        ELOG_WARN("receive message:%s,but message handler not set");
        return "disconnect";
    };

    on_close_hdl_ = [this](SocketIOClientHandler *hdl) {
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
    threads_.resize(Config::getInstance()->socket_io_thread_num);
    std::transform(threads_.begin(), threads_.end(), threads_.begin(), [this](std::thread *t) {
        return new std::thread([this]() {
            //防止多线程创建hub出现段错误
            clients_mux_.lock();
            uWS::Hub hub;
            clients_mux_.unlock();

            hub.onConnection([this](uWS::WebSocket<uWS::SERVER> *ws, uWS::HttpRequest req) {
                SocketIOClientHandler *hdl = new SocketIOClientHandler(ws, std::ref(on_message_hdl_), std::ref(on_close_hdl_));
                std::string client_id = hdl->getClient().id;
                ws->setUserData(hdl);

                std::unique_lock<std::mutex> lock(clients_mux_);
                clients_[client_id] = hdl;
            });

            hub.onMessage([this](uWS::WebSocket<uWS::SERVER> *ws, char *data, size_t len, uWS::OpCode op_codec) {
                if (op_codec == uWS::OpCode::TEXT)
                {
                    void *ptr = ws->getUserData();
                    if (ptr == nullptr)
                        return;
                    SocketIOClientHandler *hdl = reinterpret_cast<SocketIOClientHandler *>(ptr);
                    hdl->onMessage(std::string(data, len));
                }
            });

            hub.onDisconnection([this](uWS::WebSocket<uWS::SERVER> *ws, int code, char *data, size_t len) {
                void *ptr = ws->getUserData();
                if (ptr == nullptr)
                    return;

                SocketIOClientHandler *hdl = reinterpret_cast<SocketIOClientHandler *>(ptr);
                hdl->setWebSocket(nullptr);
                std::string client_id = hdl->getClient().id;
                hdl->onClose();

                std::unique_lock<std::mutex> lock(clients_mux_);
                clients_.erase(client_id);
                delete hdl;
            });

            if (Config::getInstance()->ssl)
            {
                uS::TLS::Context context = uS::TLS::createContext(Config::getInstance()->ssl_cert,
                                                                  Config::getInstance()->ssl_key,
                                                                  Config::getInstance()->ssl_passwd);
                if (!hub.listen(Config::getInstance()->ssl_port, context, uS::ListenOptions::REUSE_PORT))
                {
                    ELOG_ERROR("socket-io server(tls) listen to %d failed", Config::getInstance()->ssl_port);
                    return;
                }
            }
            else
            {
                if (!hub.listen(Config::getInstance()->port, nullptr, uS::ListenOptions::REUSE_PORT))
                {
                    ELOG_ERROR("socket-io server listen to %d failed", Config::getInstance()->ssl_port);
                    return;
                }
            }
            while (run_)
            {
                hub.getLoop()->doEpoll(500); //500ms
            }
        });
    });

    send_thread_ = std::unique_ptr<std::thread>(new std::thread([this]() {
        while (run_)
        {
            std::unique_lock<std::mutex> lock(send_mux_);
            while (!send_queue_.empty())
            {
                SIOData data = send_queue_.front();
                send_queue_.pop();
                std::unique_lock<std::mutex>(clients_mux_);
                auto it = clients_.find(data.client_id);
                if (it != clients_.end())
                    it->second->sendMessage(data.message);
            }
            send_cond_.wait(lock);
        }
    }));

    init_ = true;
    return 0;
}

void SocketIOServer::close()
{
    if (!init_)
        return;

    run_ = false;

    send_cond_.notify_all();
    send_thread_->join();
    send_thread_.reset();
    send_thread_ = nullptr;

    std::for_each(threads_.begin(), threads_.end(), [](std::thread *t) {
        t->join();
    });

    for (auto it = clients_.begin(); it != clients_.end(); it++)
    {
        it->second->onClose();
        delete it->second;
    }
    clients_.clear();

    init_ = false;
}

void SocketIOServer::sendEvent(const std::string &client_id, const std::string &msg)
{
    std::ostringstream oss;
    oss << "42";
    oss << msg;

    std::unique_lock<std::mutex> lock(send_mux_);
    send_queue_.push({client_id, oss.str()});
    send_cond_.notify_one();
}

void SocketIOServer::closeConnection(const std::string &client_id)
{
    std::unique_lock<std::mutex> lock(send_mux_);
    send_queue_.push({client_id, "41"});
    send_cond_.notify_one();
}