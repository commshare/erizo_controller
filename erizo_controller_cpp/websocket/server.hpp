#ifndef WS_SERVER_HPP
#define WS_SERVER_HPP

#include <thread>
#include <memory>
#include <functional>
#include <type_traits>
#include <mutex>

#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <logger.h>

#include "common/utils.h"
#include "common/config.h"
#include "client_handler.hpp"

template <typename T>
class WSServer
{
  DECLARE_LOGGER();

public:
  WSServer();
  ~WSServer();

  int init();
  void close();
  void setOnMessage(const std::function<std::string(ClientHandler<T> *, const std::string &)> &on_message) { on_message_ = on_message; }
  void setOnShutdown(const std::function<void(ClientHandler<T> *)> &on_shutdown) { on_shutdown_ = on_shutdown; }
  std::shared_ptr<ClientHandler<T>> getClient(const std::string &client_id)
  {
    auto it = std::find_if(clients_.begin(), clients_.end(),
                           [&](const std::pair<typename T::connection_ptr, std::shared_ptr<ClientHandler<T>>> &val) -> bool {
                             Client &client = val.second->getClient();
                             if (!client.id.compare(client_id))
                               return true;
                             return false;
                           });
    if (it != clients_.end())
      return it->second;
    return nullptr;
  }

private:
  void onConnectionOpen(T *s, websocketpp::connection_hdl hdl);
  void onConnectionClose(T *s, websocketpp::connection_hdl hdl);
  void onMessage(T *s, websocketpp::connection_hdl hdl, typename T::message_ptr msg);
  context_ptr onTLSInit(websocketpp::connection_hdl hdl);

private:
  std::unique_ptr<std::thread> thread_;
  boost::asio::io_service ios_;
  std::map<typename T::connection_ptr, std::shared_ptr<ClientHandler<T>>> clients_;
  std::function<std::string(ClientHandler<T> *, const std::string &)> on_message_;
  std::function<void(ClientHandler<T> *)> on_shutdown_;
  bool init_;
};

template <typename T>
DEFINE_LOGGER(WSServer<T>, "WSServer");

template <typename T>
context_ptr WSServer<T>::onTLSInit(websocketpp::connection_hdl hdl)
{
  context_ptr ctx(new boost::asio::ssl::context(boost::asio::ssl::context::tlsv1));
  try
  {
    ctx->set_options(boost::asio::ssl::context::default_workarounds |
                     boost::asio::ssl::context::no_sslv2 |
                     boost::asio::ssl::context::no_sslv3 |
                     boost::asio::ssl::context::single_dh_use);
    ctx->use_certificate_chain_file(Config::getInstance()->ssl_cert_);
    ctx->use_private_key_file(Config::getInstance()->ssl_key_, boost::asio::ssl::context::pem);
  }
  catch (websocketpp::exception &e)
  {
    std::cout << e.what() << std::endl;
  }
  return ctx;
}

template <typename T>
WSServer<T>::WSServer() : thread_(nullptr),
                          init_(false)
{
}

template <typename T>
WSServer<T>::~WSServer()
{
}

template <typename T>
int WSServer<T>::init()
{
  if (!std::is_same<T, server_tls>::value && !std::is_same<T, server_plain>::value)
  {
    ELOG_ERROR("Template error,only support server_tls and server_plain");
    return 1;
  }

  if (init_)
  {
    ELOG_WARN("WSServer duplicate initialize !!!");
    return 0;
  }

  thread_ = std::unique_ptr<std::thread>(new std::thread([this]() {
    try
    {
      T *endpoint = new T;
      endpoint->clear_access_channels(websocketpp::log::alevel::all);
      endpoint->clear_error_channels(websocketpp::log::alevel::all);
      endpoint->init_asio(&ios_);
      endpoint->set_message_handler(
          bind(&WSServer<T>::onMessage, this, endpoint, ::_1, ::_2));
      endpoint->set_open_handler(bind(&WSServer::onConnectionOpen, this, endpoint, ::_1));
      endpoint->set_close_handler(bind(&WSServer::onConnectionClose, this, endpoint, ::_1));
      if (std::is_same<T, server_tls>::value)
      {
        server_tls *tls = reinterpret_cast<server_tls *>(endpoint);
        tls->set_tls_init_handler(bind(&WSServer<T>::onTLSInit, this, ::_1));
        tls->listen(Config::getInstance()->ssl_port_);
      }
      else
      {
        endpoint->listen(Config::getInstance()->port_);
      }

      endpoint->start_accept();
      ios_.run();
      delete endpoint;
    }
    catch (websocketpp::exception &e)
    {
      std::cout << e.what() << std::endl;
    }
  }));
  return 0;
}

template <typename T>
void WSServer<T>::close()
{
  ios_.stop();
  thread_->join();
  thread_.reset();
  thread_ = nullptr;

  clients_.clear();
}

template <typename T>
void WSServer<T>::onMessage(T *s, websocketpp::connection_hdl hdl, typename T::message_ptr msg)
{
  ELOG_INFO("onMessage called with hdl:%#x,msg:%s", hdl.lock().get(), msg->get_payload());
}

template <typename T>
void WSServer<T>::onConnectionOpen(T *s, websocketpp::connection_hdl hdl)
{
  std::string ip;
  uint16_t port;

  typename T::connection_ptr ptr = s->get_con_from_hdl(hdl);
  if (!Utils::searchAddress(ptr->get_remote_endpoint(), ip, port))
  {
    ELOG_WARN("Parse address failed");
    return;
  }

  ELOG_INFO("New websocket connection:%#x", hdl.lock().get());
  clients_[ptr] = std::make_shared<ClientHandler<T>>(s, hdl, on_message_, on_shutdown_);
  clients_[ptr]->setAddress(ip, port);
  ELOG_INFO("SocketIO client num:%d", clients_.size());
}

template <typename T>
void WSServer<T>::onConnectionClose(T *s, websocketpp::connection_hdl hdl)
{
  ELOG_INFO("Websocket connection:%#x going to shutdown", hdl.lock().get());
  clients_[s->get_con_from_hdl(hdl)]->goingToShutdown();
  clients_.erase(s->get_con_from_hdl(hdl));
  ELOG_INFO("SocketIO client num:%d", clients_.size());
}

#endif