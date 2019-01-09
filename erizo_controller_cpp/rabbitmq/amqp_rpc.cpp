#include "amqp_rpc.h"

#include "common/config.h"
#include "common/utils.h"

constexpr int kQueueSize = 256;

DEFINE_LOGGER(AMQPRPC, "AMQPRPC");

AMQPRPC::AMQPRPC() : init_(false),
                     run_(false),
                     conn_(nullptr),
                     recv_thread_(nullptr),
                     send_thread_(nullptr),
                     reply_to_(""),
                     index_(0) {}

AMQPRPC::~AMQPRPC()
{
}

int AMQPRPC::init()
{
    if (init_)
    {
        ELOG_WARN("AMQPRPC duplicate initialize,just return!!!");
        return 0;
    }

    amqp_rpc_reply_t res;
    conn_ = amqp_new_connection();
    amqp_socket_t *socket = amqp_tcp_socket_new(conn_);
    if (!socket)
    {
        ELOG_ERROR("Creating TCP socket failed");
        return 1;
    }

    if (amqp_socket_open(socket, Config::getInstance()->rabbitmq_hostname_.c_str(), Config::getInstance()->rabbitmq_port_) != AMQP_STATUS_OK)
    {
        ELOG_ERROR("Opening TCP socket failed");
        return 1;
    }

    res = amqp_login(conn_, "/", 0, 131072, 0,
                     AMQP_SASL_METHOD_PLAIN, Config::getInstance()->rabbitmq_username_.c_str(),
                     Config::getInstance()->rabbitmq_passwd_.c_str());
    if (checkError(res))
    {
        ELOG_ERROR("Logging in failed");
        return 1;
    }

    amqp_channel_open(conn_, 1);
    res = amqp_get_rpc_reply(conn_);
    if (checkError(res))
    {
        ELOG_ERROR("Opening channel failed");
        return 1;
    }

    amqp_exchange_declare(conn_, 1, amqp_cstring_bytes(Config::getInstance()->uniquecast_exchange_.c_str()),
                          amqp_cstring_bytes("direct"), 0, 1, 0, 0,
                          amqp_empty_table);
    res = amqp_get_rpc_reply(conn_);
    if (checkError(res))
    {
        ELOG_ERROR("Declaring uniquecast exchange failed");
        return 1;
    }

    amqp_queue_declare_ok_t *r = amqp_queue_declare(
        conn_, 1, amqp_empty_bytes, 0, 0, 1, 1, amqp_empty_table);
    res = amqp_get_rpc_reply(conn_);
    if (checkError(res))
    {
        ELOG_ERROR("Declaring queue failed");
        return 1;
    }

    amqp_bytes_t queuename = amqp_bytes_malloc_dup(r->queue);
    if (queuename.bytes == NULL)
    {
        ELOG_ERROR("Out of memory while copying queue name");
        return 1;
    }

    reply_to_ = stringifyBytes(queuename);
    amqp_queue_bind(conn_, 1, queuename, amqp_cstring_bytes(Config::getInstance()->uniquecast_exchange_.c_str()),
                    queuename, amqp_empty_table);
    res = amqp_get_rpc_reply(conn_);
    if (checkError(res))
    {
        ELOG_ERROR("Binding queue failed");
        return 1;
    }

    amqp_basic_consume(conn_, 1, queuename, amqp_empty_bytes, 0, 1, 0,
                       amqp_empty_table);
    res = amqp_get_rpc_reply(conn_);
    if (checkError(res))
    {
        ELOG_ERROR("Consuming failed");
        return 1;
    }

    cb_queue_.resize(kQueueSize);

    run_ = true;
    recv_thread_ = std::unique_ptr<std::thread>(new std::thread([&]() {
        /*
         * 当rpc完成后,远端回送的数据会在这里接收,收到数据后,释放回调队列里条件变量
         * 原先阻塞的线程开始执行
         */

        while (run_)
        {
            amqp_rpc_reply_t res;
            amqp_envelope_t envelope;
            struct timeval timeout;

            amqp_maybe_release_buffers(conn_);

            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;
            res = amqp_consume_message(conn_, &envelope, &timeout, 0);
            if (AMQP_RESPONSE_NORMAL != res.reply_type)
            {
                if (res.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION && res.library_error == AMQP_STATUS_TIMEOUT)
                    continue;
                ELOG_DEBUG("Amqp consumer thread quit...");
                return;
            }

            std::string msg((const char *)envelope.message.body.bytes, envelope.message.body.len);
            handleCallback(msg);
            amqp_destroy_envelope(&envelope);
        }
        notifyAllCallbackThread();
    }));

    send_thread_ = std::unique_ptr<std::thread>(new std::thread([&]() {
        while (run_)
        {
            std::unique_lock<std::mutex> lock(send_queue_mux_);
            if (!send_queue_.empty())
            {
                AMQPData data = send_queue_.front();
                send_queue_.pop();
                callback(data.exchange, data.queuename, data.binding_key, data.msg);
            }
            else
            {
                send_cond_.wait(lock);
            }
        }
    }));

    check_thread_ = std::unique_ptr<std::thread>(new std::thread([&]() {
        while (run_)
        {
            {
                std::unique_ptr<std::mutex>(cb_queue_mux_);
                uint64_t now = Utils::getCurrentMs();
                for (AMQPCallback &cb : cb_queue_)
                {
                    if (cb.ts != 0)
                    {
                        int dur = (int)(now - cb.ts);
                        if (dur > Config::getInstance()->rabbitmq_timeout_)
                        {
                            ELOG_WARN("Rabbitmq rpc callback timeout");
                            cb.data = Json::nullValue;
                            cb.cond.notify_one();
                        }
                    }
                }
            }
            usleep(500000); //500ms
        }
    }));

    init_ = true;

    return 0;
}

void AMQPRPC::close()
{
    if (!init_)
    {
        ELOG_WARN("AMQPRPC didn't initialize,can't close!!!");
        return;
    }
    run_ = false;
    check_thread_->join();
    recv_thread_->join();
    send_cond_.notify_all();
    send_thread_->join();

    amqp_channel_close(conn_, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(conn_, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(conn_);

    amqp_channel_close(conn_, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(conn_, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(conn_);

    while (!send_queue_.empty())
        send_queue_.pop();

    cb_queue_.clear();

    init_ = false;
    conn_ = nullptr;

    check_thread_.reset();
    check_thread_ = nullptr;
    recv_thread_.reset();
    recv_thread_ = nullptr;
    send_thread_.reset();
    send_thread_ = nullptr;
}

void AMQPRPC::handleCallback(const std::string &msg)
{
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(msg, root))
        return;

    if (!root.isMember("corrID") ||
        root["corrID"].type() != Json::intValue ||
        !root.isMember("data") ||
        root["data"].type() != Json::objectValue)
        return;

    int corrid = root["corrID"].asInt();
    Json::Value data = root["data"];

    if (corrid < 0 || corrid > kQueueSize)
        return;

    std::unique_ptr<std::mutex>(cb_queue_mux_);
    AMQPCallback &cb = cb_queue_[corrid];
    cb.data = data;
    cb.cond.notify_one();
}

void AMQPRPC::addRPC(const std::string &exchange,
                     const std::string &queuename,
                     const std::string &binding_key,
                     const Json::Value &data,
                     const std::function<void(const Json::Value &)> &func)
{
    std::unique_lock<std::mutex> lock(send_queue_mux_);
    int corrid = index_ % kQueueSize;
    index_++;
    index_ = index_ % kQueueSize;

    std::thread([&, this, func, corrid]() {
        AMQPCallback &cb = cb_queue_[corrid];
        std::unique_lock<std::mutex> lock(cb.mux);
        if (cb.ts == 0)
        {
            cb.ts = Utils::getCurrentMs();
            cb.data = Json::nullValue;
            cb.cond.wait(lock);
            func(cb.data);
            cb.ts = 0;
        }
        else
        {
            func(Json::nullValue);
        }
    })
        .detach();

    Json::Value root;
    root["corrID"] = corrid;
    root["replyTo"] = reply_to_;
    root["data"] = data;
    Json::FastWriter writer;
    std::string msg = writer.write(root);

    send_queue_.push({exchange, queuename, binding_key, msg});
    send_cond_.notify_one();
}

void AMQPRPC::sendMessage(const std::string &exchange,
                          const std::string &queuename,
                          const std::string &binding_key,
                          const Json::Value &data)
{
    Json::Value root;
    root["data"] = data;
    root["corrID"] = Json::intValue;
    root["replyTo"] = Json::stringValue;
    Json::FastWriter writer;
    std::string msg = writer.write(root);

    std::unique_lock<std::mutex> lock(send_queue_mux_);
    send_queue_.push({exchange, queuename, binding_key, msg});
    send_cond_.notify_one();
}

std::string AMQPRPC::stringifyBytes(amqp_bytes_t bytes)
{
    std::ostringstream oss;
    uint8_t *data = (uint8_t *)bytes.bytes;

    for (size_t i = 0; i < bytes.len; i++)
    {
        if (data[i] >= 32 && data[i] != 127)
        {
            oss << data[i];
        }
        else
        {
            oss << '\\';
            oss << ('0' + (data[i] >> 6));
            oss << ('0' + (data[i] >> 3 & 0x7));
            oss << ('0' + (data[i] & 0x7));
        }
    }
    return oss.str();
}

int AMQPRPC::checkError(amqp_rpc_reply_t x)
{
    switch (x.reply_type)
    {
    case AMQP_RESPONSE_NORMAL:
        return 0;

    case AMQP_RESPONSE_NONE:
        ELOG_ERROR("missing RPC reply type!");
        break;

    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
        ELOG_ERROR("%s", amqp_error_string2(x.library_error));
        break;

    case AMQP_RESPONSE_SERVER_EXCEPTION:
        switch (x.reply.id)
        {
        case AMQP_CONNECTION_CLOSE_METHOD:
        {
            amqp_connection_close_t *m =
                (amqp_connection_close_t *)x.reply.decoded;
            ELOG_ERROR("server connection error %uh, message: %.*s",
                       m->reply_code, (int)m->reply_text.len,
                       (char *)m->reply_text.bytes);
            break;
        }
        case AMQP_CHANNEL_CLOSE_METHOD:
        {
            amqp_channel_close_t *m = (amqp_channel_close_t *)x.reply.decoded;
            ELOG_ERROR("server channel error %uh, message: %.*s",
                       m->reply_code, (int)m->reply_text.len,
                       (char *)m->reply_text.bytes);
            break;
        }
        default:
            ELOG_ERROR("unknown server error, method id 0x%08X",
                       x.reply.id);
            break;
        }
        break;
    }
    return 1;
}

void AMQPRPC::notifyAllCallbackThread()
{
    std::unique_lock<std::mutex>(cb_queue_mux_);
    for (AMQPCallback &cb : cb_queue_)
        cb.cond.notify_all();
}

int AMQPRPC::callback(const std::string &exchange, const std::string &queuename, const std::string &binding_key, const std::string &send_msg)
{
    amqp_basic_properties_t props;
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG;
    props.content_type = amqp_cstring_bytes("application/json");
    props.delivery_mode = 2;
    props.correlation_id = amqp_cstring_bytes("1");
    props.reply_to = amqp_bytes_malloc_dup(amqp_cstring_bytes(queuename.c_str()));
    if (props.reply_to.bytes == NULL)
    {
        ELOG_ERROR("Out of memory while copying queue name");
        return 1;
    }

    amqp_basic_publish(conn_, 1, amqp_cstring_bytes(exchange.c_str()),
                       amqp_cstring_bytes(binding_key.c_str()), 0, 0,
                       &props, amqp_cstring_bytes(send_msg.c_str()));
    amqp_bytes_free(props.reply_to);
    return 0;
}