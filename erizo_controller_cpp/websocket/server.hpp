#ifndef WS_SERVER_HPP
#define WS_SERVER_HPP

#include <thread>
#include <memory>
#include <functional>
#include <type_traits>
#include <mutex>

#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>

#include "common/utils.h"
#include "common/config.h"
#include "common/logger.h"
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
  std::shared_ptr<ClientHandler<T>> getClient(const std::string &client_id);

private:
  void onConnectionOpen(T *s, websocketpp::connection_hdl hdl);
  void onConnectionClose(T *s, websocketpp::connection_hdl hdl);
  void onMessage(T *s, websocketpp::connection_hdl hdl, typename T::message_ptr msg);
  context_ptr onTLSInit(websocketpp::connection_hdl hdl);

private:
  std::function<std::string(ClientHandler<T> *, const std::string &)> on_message_;
  std::function<void(ClientHandler<T> *)> on_shutdown_;
  boost::asio::io_service ios_;
  std::mutex mux_;
  std::map<typename T::connection_ptr, std::shared_ptr<ClientHandler<T>>> clients_map_;
  std::map<std::string, std::shared_ptr<ClientHandler<T>>> clients_;
  std::unique_ptr<std::thread> thread_;
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
    return 1;

  if (init_)
    return 0;

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
  init_ = true;
  return 0;
}

template <typename T>
void WSServer<T>::close()
{
  if (!init_)
    return;
  ios_.stop();
  thread_->join();
  thread_.reset();
  thread_ = nullptr;
  clients_map_.clear();
  clients_.clear();
  init_ = false;
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

  std::unique_lock<std::mutex> lock(mux_);
  std::shared_ptr<ClientHandler<T>> client = std::make_shared<ClientHandler<T>>(s, hdl, on_message_, on_shutdown_);
  client->setAddress(ip, port);
  clients_map_[ptr] = client;
  clients_[client->getClientId()] = client;
  ELOG_INFO("new websocket connection:%#x,client num:%d", hdl.lock().get(), clients_map_.size());
}

template <typename T>
void WSServer<T>::onConnectionClose(T *s, websocketpp::connection_hdl hdl)
{
  std::unique_lock<std::mutex>(mux_);
  auto it = clients_map_.find(s->get_con_from_hdl(hdl));
  if (it != clients_map_.end())
  {
    it->second->goingToShutdown();
    clients_.erase(it->second->getClientId());
    clients_map_.erase(it);
    ELOG_INFO("websocket connection:%#x going to shutdown,client num:%d", hdl.lock().get(), clients_map_.size());
  }
}

template <typename T>
std::shared_ptr<ClientHandler<T>> WSServer<T>::getClient(const std::string &client_id)
{
  std::unique_lock<std::mutex> lock(mux_);
  auto it = clients_.find(client_id);
  if (it != clients_.end())
    return it->second;
  return nullptr;
}

#endif