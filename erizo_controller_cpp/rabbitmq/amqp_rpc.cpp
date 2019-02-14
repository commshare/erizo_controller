#include "amqp_rpc.h"

#include "common/config.h"
#include "common/utils.h"
#include "amqp_cli.h"

constexpr int kQueueSize = 256;

DEFINE_LOGGER(AMQPRPC, "AMQPRPC");

AMQPRPC::AMQPRPC() : reply_to_(""),
                     index_(0),
                     recv_thread_(nullptr),
                     send_thread_(nullptr),
                     check_thread_(nullptr),
                     run_(false),
                     init_(false) {}

AMQPRPC::~AMQPRPC() {}

int AMQPRPC::init(const std::string &binding_key)
{
    if (init_)
        return 0;

    amqp_cli_ = std::unique_ptr<AMQPCli>(new AMQPCli);
    if (amqp_cli_->init(Config::getInstance()->uniquecast_exchange, "direct", binding_key))
    {
        ELOG_ERROR("amqp-cli initialize failed");
        return 1;
    }
    reply_to_ = amqp_cli_->getReplyTo();
    cb_queue_.resize(kQueueSize);

    run_ = true;
    recv_thread_ = std::unique_ptr<std::thread>(new std::thread([this]() {
        amqp_connection_state_t conn = amqp_cli_->getConnection();
        while (run_)
        {
            amqp_rpc_reply_t res;
            amqp_envelope_t envelope;
            struct timeval timeout;

            amqp_maybe_release_buffers(conn);

            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;
            res = amqp_consume_message(conn, &envelope, &timeout, 0);
            if (AMQP_RESPONSE_NORMAL != res.reply_type)
            {
                if (res.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION && res.library_error == AMQP_STATUS_TIMEOUT)
                    continue;
                return;
            }

            std::string msg((const char *)envelope.message.body.bytes, envelope.message.body.len);
            handleCallback(msg);
            amqp_destroy_envelope(&envelope);
        }
    }));

    send_thread_ = std::unique_ptr<std::thread>(new std::thread([this]() {
        while (run_)
        {
            std::unique_lock<std::mutex> lock(send_queue_mux_);
            while (!send_queue_.empty())
            {
                AMQPData data = send_queue_.front();
                send_queue_.pop();
                send(data.exchange, data.queuename, data.binding_key, data.msg);
            }
            send_cond_.wait(lock);
        }
    }));

    check_thread_ = std::unique_ptr<std::thread>(new std::thread([this]() {
        while (run_)
        {
            for (AMQPCallback &cb : cb_queue_)
            {
                uint64_t now = Utils::getCurrentMs();
                std::unique_lock<std::mutex>(cb.mux);
                if (cb.ts > 0)
                {
                    if (now - cb.ts > (uint64_t)Config::getInstance()->rabbitmq_timeout)
                    {
                        cb.func(Json::nullValue);
                        cb.ts = 0;
                        ELOG_WARN("rpc timeout,dump %s", cb.dump.c_str());
                    }
                }
            }
            usleep(500000);
        }
    }));

    init_ = true;
    return 0;
}

void AMQPRPC::close()
{
    if (!init_)
        return;

    run_ = false;
    check_thread_->join();
    check_thread_.reset();
    check_thread_ = nullptr;

    recv_thread_->join();
    recv_thread_.reset();
    recv_thread_ = nullptr;

    send_cond_.notify_all();
    send_thread_->join();
    send_thread_.reset();
    send_thread_ = nullptr;

    amqp_cli_->close();
    amqp_cli_.reset();
    amqp_cli_ = nullptr;

    cb_queue_.clear();
    while (!send_queue_.empty())
        send_queue_.pop();

    init_ = false;
}

void AMQPRPC::handleCallback(const std::string &msg)
{
    Json::Value root;
    Json::Reader reader(Json::Features::strictMode());
    if (!reader.parse(msg, root))
        return;

    if (!root.isMember("corrID") ||
        root["corrID"].type() != Json::intValue ||
        !root.isMember("data") ||
        root["data"].type() != Json::objectValue)
    {
        ELOG_ERROR("json parse [corrID/data] failed,dump %s", msg);
        return;
    }
    int corrid = root["corrID"].asInt();
    Json::Value data = root["data"];

    if (corrid < 0 || corrid > kQueueSize)
    {
        ELOG_ERROR("rpc callback corrid error");
        return;
    }

    AMQPCallback &cb = cb_queue_[corrid];
    std::unique_lock<std::mutex>(cb.mux);
    if (cb.ts > 0)
    {
        cb.func(data);
        cb.ts = 0;
    }
    else
    {
        ELOG_ERROR("rpc callback not exist");
    }
}

void AMQPRPC::rpc(const std::string &exchange,
                  const std::string &queuename,
                  const std::string &binding_key,
                  const Json::Value &data,
                  const std::function<void(const Json::Value &)> &func)
{
    int corrid = index_++ % kQueueSize;

    {
        AMQPCallback &cb = cb_queue_[corrid];
        std::unique_lock<std::mutex> lock(cb.mux);
        if (cb.ts == 0)
        {
            cb.dump = Utils::dumpJson(data);
            cb.ts = Utils::getCurrentMs();
            cb.func = func;
        }
        else
        {
            ELOG_ERROR("rpc callback queue fill");
            func(Json::nullValue);
        }
    }

    Json::Value root;
    root["corrID"] = corrid;
    root["replyTo"] = reply_to_;
    root["data"] = data;
    Json::FastWriter writer;
    std::string msg = writer.write(root);

    std::unique_lock<std::mutex> lock(send_queue_mux_);
    send_queue_.push({exchange, queuename, binding_key, msg});
    send_cond_.notify_one();
}

int AMQPRPC::rpc(const std::string &queuename, const Json::Value &data)
{
    int ret;
    std::atomic<bool> callback_done;
    int try_time = 3;
    do
    {
        try_time--;
        callback_done = false;
        rpc(Config::getInstance()->uniquecast_exchange, queuename, queuename, data, [this, &ret, &callback_done](const Json::Value &root) {
            if (root.type() == Json::nullValue)
            {
                ret = 1;
                callback_done = true;
                return;
            }
            if (!root.isMember("ret") ||
                root["ret"].type() != Json::intValue)
            {
                ret = 1;
                callback_done = true;
                return;
            }
            ret = root["ret"].asInt();
            callback_done = true;
        });
        while (!callback_done)
            usleep(0);
    } while (ret && try_time);
    return ret;
}

void AMQPRPC::rpcNotReply(const std::string &queuename, const Json::Value &data)
{
    Json::Value root;
    root["data"] = data;
    Json::FastWriter writer;
    std::string msg = writer.write(root);

    std::unique_lock<std::mutex> lock(send_queue_mux_);
    send_queue_.push({Config::getInstance()->uniquecast_exchange, queuename, queuename, msg});
    send_cond_.notify_one();
}

int AMQPRPC::send(const std::string &exchange, const std::string &queuename, const std::string &binding_key, const std::string &send_msg)
{
    amqp_connection_state_t conn = amqp_cli_->getConnection();
    amqp_basic_properties_t props;
    props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG;
    props.content_type = amqp_cstring_bytes("application/json");
    props.delivery_mode = 2;
    props.correlation_id = amqp_cstring_bytes("1");
    props.reply_to = amqp_bytes_malloc_dup(amqp_cstring_bytes(queuename.c_str()));
    if (props.reply_to.bytes == NULL)
    {
        ELOG_ERROR("out of memory while copying queue name");
        return 1;
    }

    amqp_basic_publish(conn, 1, amqp_cstring_bytes(exchange.c_str()),
                       amqp_cstring_bytes(binding_key.c_str()), 0, 0,
                       &props, amqp_cstring_bytes(send_msg.c_str()));
    amqp_bytes_free(props.reply_to);
    return 0;
}