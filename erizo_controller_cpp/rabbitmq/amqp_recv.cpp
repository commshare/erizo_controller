#include "amqp_recv.h"

#include <unistd.h>

#include "common/config.h"

DEFINE_LOGGER(AMQPRecv, "AMQPRecv");
static Config *config = Config::getInstance();

AMQPRecv::AMQPRecv() : reply_to_(""),
                       conn_(nullptr),
                       recv_thread_(nullptr),
                       run_(false),
                       init_(false)
{
}

AMQPRecv::~AMQPRecv() {}

int AMQPRecv::checkError(amqp_rpc_reply_t x)
{
    switch (x.reply_type)
    {
    case AMQP_RESPONSE_NORMAL:
        return 0;

    case AMQP_RESPONSE_NONE:
        ELOG_ERROR("missing rpc reply type!");
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

int AMQPRecv::init(const std::function<void(const std::string &msg)> &func)
{
    if (init_)
        return 0;

    amqp_rpc_reply_t res;
    conn_ = amqp_new_connection();
    amqp_socket_t *socket = amqp_tcp_socket_new(conn_);
    if (!socket)
    {
        ELOG_ERROR("create tcp socket failed");
        return 1;
    }

    if (amqp_socket_open(socket, config->rabbitmq_hostname.c_str(), config->rabbitmq_port) != AMQP_STATUS_OK)
    {
        ELOG_ERROR("open tcp socket failed");
        return 1;
    }

    res = amqp_login(conn_, "/", 0, 131072, 0,
                     AMQP_SASL_METHOD_PLAIN, config->rabbitmq_username.c_str(),
                     config->rabbitmq_passwd.c_str());
    if (checkError(res))
    {
        ELOG_ERROR("login failed");
        return 1;
    }

    amqp_channel_open(conn_, 1);
    res = amqp_get_rpc_reply(conn_);
    if (checkError(res))
    {
        ELOG_ERROR("open channel failed");
        return 1;
    }

    amqp_queue_declare_ok_t *r = amqp_queue_declare(
        conn_, 1, amqp_empty_bytes, 0, 0, 1, 1, amqp_empty_table);
    res = amqp_get_rpc_reply(conn_);
    if (checkError(res))
    {
        ELOG_ERROR("declare queue failed");
        return 1;
    }

    amqp_bytes_t queuename = amqp_bytes_malloc_dup(r->queue);
    if (queuename.bytes == NULL)
    {
        ELOG_ERROR("out of memory while copying queue name");
        return 1;
    }

    reply_to_ = stringifyBytes(queuename);
    amqp_queue_bind(conn_, 1, queuename, amqp_cstring_bytes(Config::getInstance()->uniquecast_exchange.c_str()),
                    queuename, amqp_empty_table);
    res = amqp_get_rpc_reply(conn_);
    if (checkError(res))
    {
        ELOG_ERROR("bind queue failed");
        return 1;
    }

    amqp_basic_consume(conn_, 1, queuename, amqp_empty_bytes, 0, 1, 0,
                       amqp_empty_table);
    res = amqp_get_rpc_reply(conn_);
    if (checkError(res))
    {
        ELOG_ERROR("consume failed");
        return 1;
    }

    run_ = true;
    recv_thread_ = std::unique_ptr<std::thread>(new std::thread([this, func]() {
        while (run_)
        {
            amqp_rpc_reply_t res;
            amqp_envelope_t envelope;
            struct timeval timeout;

            amqp_maybe_release_buffers(conn_);

            timeout.tv_sec = 0;
            timeout.tv_usec = 100000; //100ms
            res = amqp_consume_message(conn_, &envelope, &timeout, 0);
            if (AMQP_RESPONSE_NORMAL != res.reply_type)
            {
                if (res.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION && res.library_error == AMQP_STATUS_TIMEOUT)
                    continue;
                return;
            }

            std::string msg((const char *)envelope.message.body.bytes, envelope.message.body.len);
            func(msg);
            amqp_destroy_envelope(&envelope);
        }
    }));

    init_ = true;

    return 0;
}

void AMQPRecv::close()
{
    if (!init_)
        return;

    run_ = false;
    recv_thread_->join();
    recv_thread_.reset();
    recv_thread_ = nullptr;

    amqp_channel_close(conn_, 1, AMQP_REPLY_SUCCESS);
    amqp_connection_close(conn_, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(conn_);
    conn_ = nullptr;

    init_ = false;
}

std::string AMQPRecv::stringifyBytes(amqp_bytes_t bytes)
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
