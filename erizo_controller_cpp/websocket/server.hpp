// #ifndef WEBSOCKET_CLIENT_HANDLER_HPP
// #define WEBSOCKET_CLIENT_HANDLER_HPP

// #include <thread>
// #include <mutex>
// #include <memory>

// #include <websocketpp/config/asio.hpp>
// #include <websocketpp/server.hpp>
// #include <json/json.h>

// #include "thread/thread_pool.h"
// #include "thread/worker.h"
// #include "model/client.h"
// #include "common/utils.h"
// #include "common/logger.h"
// #include "common/config.h"

// using websocketpp::lib::bind;
// using websocketpp::lib::placeholders::_1;
// using websocketpp::lib::placeholders::_2;

// typedef websocketpp::server<websocketpp::config::asio> server_plain;
// typedef websocketpp::server<websocketpp::config::asio_tls> server_tls;
// typedef websocketpp::lib::shared_ptr<boost::asio::ssl::context> context_ptr;

// template <typename T>
// class ClientHandler;

// template <typename T>
// class WSServer
// {
//   DECLARE_LOGGER();
//   struct WSData
//   {
//     websocketpp::connection_hdl hdl;
//     std::string msg;
//   };

// public:
//   WSServer();
//   ~WSServer();

//   int init();
//   void close();
//   void setOnMessage(const std::function<std::string(ClientHandler<T> *, const std::string &)> &on_message) { on_message_ = on_message; }
//   void setOnShutdown(const std::function<void(ClientHandler<T> *)> &on_shutdown) { on_shutdown_ = on_shutdown; }
//   std::shared_ptr<ClientHandler<T>> getClient(const std::string &client_id);

//   void sendMessage(websocketpp::connection_hdl hdl, std::string msg)
//   {
//     send_queue_.push({hdl, msg});
//     send_cond_.notify_one();
//   }

//   typename T::connection_ptr get_con_from_hdl(websocketpp::connection_hdl hdl)
//   {
//     return server_->get_con_from_hdl(hdl);
//   }

// private:
//   void onConnectionOpen(websocketpp::connection_hdl hdl);
//   void onConnectionClose(websocketpp::connection_hdl hdl);
//   void onMessage(websocketpp::connection_hdl hdl, typename T::message_ptr msg);
//   context_ptr onTLSInit(websocketpp::connection_hdl hdl);

// private:
//   std::function<std::string(ClientHandler<T> *, const std::string &)> on_message_;
//   std::function<void(ClientHandler<T> *)> on_shutdown_;
//   boost::asio::io_service ios_;

//   std::unique_ptr<erizo::ThreadPool> thread_pool_;

//   std::mutex clients_mux_;
//   //std::map<typename T::connection_ptr, std::shared_ptr<ClientHandler<T>>> conns_clients_map_;
//   std::map<void*, std::shared_ptr<ClientHandler<T>>> conns_clients_map_;
//   std::map<std::string, std::shared_ptr<ClientHandler<T>>> ids_clients_map_;

//   std::mutex send_mux_;
//   std::condition_variable send_cond_;
//   std::queue<WSData> send_queue_;
//   std::unique_ptr<std::thread> send_thread_;
//   std::atomic<bool> run_;

//   std::unique_ptr<std::thread> access_thread_;

//   T *server_;
//   bool init_;
// };

// template <typename T>
// DEFINE_LOGGER(WSServer<T>, "WSServer");

// template <typename T>
// context_ptr WSServer<T>::onTLSInit(websocketpp::connection_hdl hdl)
// {
//   context_ptr ctx(new boost::asio::ssl::context(boost::asio::ssl::context::tlsv1));
//   try
//   {
//     ctx->set_options(boost::asio::ssl::context::default_workarounds |
//                      boost::asio::ssl::context::no_sslv2 |
//                      boost::asio::ssl::context::no_sslv3 |
//                      boost::asio::ssl::context::single_dh_use);
//     ctx->use_certificate_chain_file(Config::getInstance()->ssl_cert_);
//     ctx->use_private_key_file(Config::getInstance()->ssl_key_, boost::asio::ssl::context::pem);
//   }
//   catch (websocketpp::exception &e)
//   {
//     std::cout << e.what() << std::endl;
//   }
//   return ctx;
// }

// template <typename T>
// WSServer<T>::WSServer() : thread_pool_(nullptr),
//                           send_thread_(nullptr),
//                           run_(false),
//                           server_(nullptr),
//                           init_(false)
// {
// }

// template <typename T>
// WSServer<T>::~WSServer()
// {
// }

// template <typename T>
// int WSServer<T>::init()
// {
//   if (!std::is_same<T, server_tls>::value && !std::is_same<T, server_plain>::value)
//     return 1;

//   if (init_)
//     return 0;

//   thread_pool_ = std::unique_ptr<erizo::ThreadPool>(new erizo::ThreadPool(Config::getInstance()->worker_num_));
//   thread_pool_->start();

//   run_ = true;
//   server_ = new T;
//   access_thread_ = std::unique_ptr<std::thread>(new std::thread([&]() {
//     try
//     {

//       server_->clear_access_channels(websocketpp::log::alevel::all);
//       server_->clear_error_channels(websocketpp::log::alevel::all);
//       server_->init_asio(&ios_);
//       server_->set_message_handler(
//           bind(&WSServer<T>::onMessage, this, ::_1, ::_2));
//       server_->set_open_handler(bind(&WSServer::onConnectionOpen, this, ::_1));
//       server_->set_close_handler(bind(&WSServer::onConnectionClose, this, ::_1));
//       if (std::is_same<T, server_tls>::value)
//       {
//         server_tls *tls = reinterpret_cast<server_tls *>(server_);
//         tls->set_tls_init_handler(bind(&WSServer<T>::onTLSInit, this, ::_1));
//         tls->listen(Config::getInstance()->ssl_port_);
//       }
//       else
//       {
//         server_->listen(Config::getInstance()->port_);
//       }

//       server_->start_accept();
//       ios_.run();
//     }
//     catch (websocketpp::exception &e)
//     {
//       std::cout << e.what() << std::endl;
//     }
//   }));

//   send_thread_ = std::unique_ptr<std::thread>(new std::thread([&]() {
//     while (run_)
//     {
//       std::unique_lock<std::mutex> lock(send_mux_);
//       if (!send_queue_.empty())
//       {
//         WSData data = send_queue_.front();
//         send_queue_.pop();

//         std::unique_ptr<std::mutex>(clients_mux_);
//         printf("ffffff :%#x\n",data.hdl.lock().get());
//         //if (conns_clients_map_.find(server_->get_con_from_hdl(data.hdl)) != conns_clients_map_.end())
//         if (conns_clients_map_.find(data.hdl.lock().get()) != conns_clients_map_.end())
//         {
//           printf("msg--->:%s\n",data.msg.c_str());
//           server_->send(data.hdl, data.msg, websocketpp::frame::opcode::TEXT);
//         }
//       }
//       else
//       {
//         send_cond_.wait(lock);
//       }
//     }
//   }));

//   init_ = true;
//   return 0;
// }

// template <typename T>
// void WSServer<T>::close()
// {
//   if (!init_)
//     return;

//   ios_.stop();

//   run_ = false;
//   send_cond_.notify_all();
//   send_thread_->join();
//   send_thread_.reset();
//   send_thread_ = nullptr;

//   access_thread_->join();
//   access_thread_.reset();
//   access_thread_ = nullptr;

//   thread_pool_->close();
//   thread_pool_.reset();
//   thread_pool_ = nullptr;

//   conns_clients_map_.clear();
//   ids_clients_map_.clear();

//   delete server_;
//   server_ = nullptr;

//   init_ = false;
// }

// template <typename T>
// void WSServer<T>::onMessage(websocketpp::connection_hdl hdl, typename T::message_ptr msg)
// {
// }

// template <typename T>
// void WSServer<T>::onConnectionOpen(websocketpp::connection_hdl hdl)
// {
//   std::string ip;
//   uint16_t port;

//   typename T::connection_ptr ptr = server_->get_con_from_hdl(hdl);
//   if (!Utils::searchAddress(ptr->get_remote_endpoint(), ip, port))
//   {
//     ELOG_WARN("Parse address failed");
//     return;
//   }

//   std::unique_lock<std::mutex> lock(clients_mux_);
//   std::shared_ptr<ClientHandler<T>> client = std::make_shared<ClientHandler<T>>(this,
//                                                                                 hdl,
//                                                                                 thread_pool_->getLessUsedWorker(),
//                                                                                 on_message_,
//                                                                                 on_shutdown_);
//   client->setAddress(ip, port);
//   //conns_clients_map_[ptr] = client;
//   conns_clients_map_[hdl.lock().get()] = client;
//   ids_clients_map_[client->getClientId()] = client;

//   ELOG_INFO("new websocket connection:%#x,client num:%d", hdl.lock().get(), conns_clients_map_.size());
// }

// template <typename T>
// void WSServer<T>::onConnectionClose(websocketpp::connection_hdl hdl)
// {
//   std::unique_lock<std::mutex>(clients_mux_);
//   //auto it = conns_clients_map_.find(server_->get_con_from_hdl(hdl));
//   auto it = conns_clients_map_.find(hdl.lock().get());
//   if (it != conns_clients_map_.end())
//   {
//     it->second->goingToShutdown();
//     ids_clients_map_.erase(it->second->getClientId());
//     conns_clients_map_.erase(hdl.lock().get());

//     ELOG_INFO("websocket connection:%#x going to shutdown,client num:%d", hdl.lock().get(), conns_clients_map_.size());
//   }
// }

// template <typename T>
// std::shared_ptr<ClientHandler<T>> WSServer<T>::getClient(const std::string &client_id)
// {
//   std::unique_lock<std::mutex> lock(clients_mux_);
//   auto it = ids_clients_map_.find(client_id);
//   if (it != ids_clients_map_.end())
//     return it->second;
//   return nullptr;
// }

// enum SOCKET_IO_FRAME_TYPE
// {
//   frame_open = 0,
//   frame_close = 1,
//   frame_ping = 2,
//   frame_pong = 3,
//   frame_message = 4,
//   frame_upgrade = 5,
//   frame_noop = 6
// };

// enum SOCKET_IO_MSG_TYPE
// {
//   type_connect = 0,
//   type_disconnect = 1,
//   type_event = 2,
//   type_ack = 3,
//   type_error = 4,
//   type_binary_event = 5,
//   type_binary_ack = 6,
//   type_undetermined = 0x10
// };

// template <typename T>
// class ClientHandler
// {
//   DECLARE_LOGGER();

// public:
//   ClientHandler(WSServer<T> *server,
//                 websocketpp::connection_hdl hdl,
//                 std::shared_ptr<erizo::Worker> worker,
//                 const std::function<std::string(ClientHandler<T> *, const std::string &)> &on_message,
//                 const std::function<void(ClientHandler<T> *)> &on_shutdown);
//   ~ClientHandler();

//   void sendEvent(const std::string &msg);

//   void setAddress(const std::string &ip, uint16_t port)
//   {
//     client_.client_ip = ip;
//     client_.client_port = port;
//   }

//   void goingToShutdown()
//   {
//     closed = true;
//     on_shutdown_(this);
//   }

//   Client &getClient()
//   {
//     return client_;
//   }

//   std::string getClientId()
//   {
//     return client_.id;
//   }

//   void asyncTask(const std::function<void()> &func)
//   {
//     worker_->task(func);
//   }

// private:
//   void onMessage(websocketpp::connection_hdl hdl, typename T::message_ptr msg);
//   void handleMessage(const std::string &payload);
//   void handlePing();

// private:
//   WSServer<T> *server_;
//   websocketpp::connection_hdl hdl_;
//   std::shared_ptr<erizo::Worker> worker_;
//   std::function<std::string(ClientHandler<T> *, const std::string &)> on_message_;
//   std::function<void(ClientHandler<T> *)> on_shutdown_;
//   std::atomic<bool> closed;
//   Client client_;
// };

// template <typename T>
// DEFINE_LOGGER(ClientHandler<T>, "ClientHandler");

// template <typename T>
// ClientHandler<T>::ClientHandler(WSServer<T> *server,
//                                 websocketpp::connection_hdl hdl,
//                                 std::shared_ptr<erizo::Worker> worker,
//                                 const std::function<std::string(ClientHandler<T> *, const std::string &)> &on_message,
//                                 const std::function<void(ClientHandler<T> *)> &on_shutdown) : server_(server),
//                                                                                               hdl_(hdl),
//                                                                                               worker_(worker),
//                                                                                               on_message_(on_message),
//                                                                                               on_shutdown_(on_shutdown),
//                                                                                               closed(false)
// {
//   client_.id = Utils::getUUID();

//   typename T::connection_ptr conn = server_->get_con_from_hdl(hdl_);
//   conn->set_message_handler(bind(&ClientHandler::onMessage, this, ::_1, ::_2));

//   Json::Value handshake;
//   handshake["sid"] = Utils::getUUID();
//   handshake["upgrades"] = Json::arrayValue;
//   handshake["pingInterval"] = 50000;
//   handshake["pingTimeout"] = 10000;

//   Json::FastWriter writer;
//   std::string msg = "0" + writer.write(handshake);

//   server_->sendMessage(hdl_, msg);
//   server_->sendMessage(hdl_, "40");
// }

// template <typename T>
// ClientHandler<T>::~ClientHandler()
// {
// }

// template <typename T>
// void ClientHandler<T>::onMessage(websocketpp::connection_hdl hdl, typename T::message_ptr msg)
// {
//   asyncTask([=]() {
//     std::string payload = msg->get_payload();
//     SOCKET_IO_FRAME_TYPE frame_type = (SOCKET_IO_FRAME_TYPE)(payload[0] - '0');
//     switch (frame_type)
//     {
//     case SOCKET_IO_FRAME_TYPE::frame_ping:
//       handlePing();
//       break;
//     case SOCKET_IO_FRAME_TYPE::frame_message:
//       handleMessage(payload);
//       break;
//     default:
//       break;
//     }
//   });
// }

// template <typename T>
// void ClientHandler<T>::handlePing()
// {
//   server_->sendMessage(hdl_, "3");
// }

// template <typename T>
// void ClientHandler<T>::handleMessage(const std::string &payload)
// {
//   SOCKET_IO_MSG_TYPE msg_type = (SOCKET_IO_MSG_TYPE)(payload[1] - '0');
//   if (msg_type != SOCKET_IO_MSG_TYPE::type_event)
//     return;

//   int mid;
//   bool has_mid = false;

//   size_t pos = payload.find_first_of('[');
//   if (pos == payload.npos)
//     return;

//   if (pos > 2)
//   {
//     has_mid = true;
//     mid = std::stoi(payload.substr(2, pos));
//   }

//   std::string event = payload.substr(pos);
//   std::string msg = on_message_(this, event);
//   if (!msg.compare("shutdown"))
//   {
//     server_->sendMessage(hdl_, "41");
//     return;
//   }
//   else if (!msg.compare("notreply"))
//   {
//     return;
//   }
//   else
//   {
//     std::ostringstream oss;
//     oss << "43";
//     if (has_mid)
//       oss << mid;
//     oss << msg;
//     server_->sendMessage(hdl_, oss.str());
//   }
// }

// template <typename T>
// void ClientHandler<T>::sendEvent(const std::string &msg)
// {
//   std::ostringstream oss;
//   oss << "42";
//   oss << msg;
//   server_->sendMessage(hdl_, oss.str());
// }

// #endif