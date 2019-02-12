#include "amqp_recv.h"

#include <unistd.h>

#include "common/config.h"
#include "amqp_cli.h"

DEFINE_LOGGER(AMQPRecv, "AMQPRecv");

AMQPRecv::AMQPRecv() : reply_to_(""),
                       amqp_cli_(nullptr),
                       recv_thread_(nullptr),
                       run_(false),
                       init_(false)
{
}

AMQPRecv::~AMQPRecv() {}

int AMQPRecv::initUniquecast(const std::function<void(const std::string &msg)> &func)
{
    if (init_)
        return 0;

    amqp_cli_ = std::unique_ptr<AMQPCli>(new AMQPCli());
    if (amqp_cli_->init(Config::getInstance()->uniquecast_exchange))
    {
        ELOG_ERROR("amqp-cli initialize failed");
        return 1;
    }

    reply_to_ = amqp_cli_->getReplyTo();

    run_ = true;
    recv_thread_ = std::unique_ptr<std::thread>(new std::thread([this, func]() {
        amqp_connection_state_t conn = amqp_cli_->getConnection();
        while (run_)
        {
            amqp_rpc_reply_t res;
            amqp_envelope_t envelope;
            struct timeval timeout;

            amqp_maybe_release_buffers(conn);

            timeout.tv_sec = 0;
            timeout.tv_usec = 100000; //100ms
            res = amqp_consume_message(conn, &envelope, &timeout, 0);
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

int AMQPRecv::initBoardcast(const std::function<void(const std::string &msg)> &func)
{
    if (init_)
        return 0;

    amqp_cli_ = std::unique_ptr<AMQPCli>(new AMQPCli());
    if (amqp_cli_->init(Config::getInstance()->boardcast_exchange, "topic", "erizo_controller"))
    {
        ELOG_ERROR("amqp-cli initialize failed");
        return 1;
    }

    reply_to_ = amqp_cli_->getReplyTo();

    run_ = true;
    recv_thread_ = std::unique_ptr<std::thread>(new std::thread([this, func]() {
        amqp_connection_state_t conn = amqp_cli_->getConnection();
        while (run_)
        {
            amqp_rpc_reply_t res;
            amqp_envelope_t envelope;
            struct timeval timeout;

            amqp_maybe_release_buffers(conn);

            timeout.tv_sec = 0;
            timeout.tv_usec = 100000; //100ms
            res = amqp_consume_message(conn, &envelope, &timeout, 0);
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

    amqp_cli_->close();
    amqp_cli_.reset();
    amqp_cli_ = nullptr;

    init_ = false;
}

const std::string &AMQPRecv::getReplyTo()
{
    return reply_to_;
}
