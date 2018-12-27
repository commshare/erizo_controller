#include "erizo_controller.h"

DEFINE_LOGGER(ErizoController, "ErizoController");

ErizoController::ErizoController() : redis_(nullptr),
                                     ws_tls_(nullptr),
                                     ws_(nullptr),
                                     amqp_(nullptr),
                                     amqp_signaling_(nullptr),
                                     init_(false)
{
}

ErizoController::~ErizoController()
{
}

int ErizoController::init()
{
    if (init_)
        return 0;

    redis_ = std::make_shared<RedisHelper>();
    if (redis_->init())
    {
        ELOG_ERROR("Redis initialize failed");
        return 1;
    }

    ws_tls_ = std::make_shared<WSServer<server_tls>>();
    if (ws_tls_->init())
    {
        ELOG_ERROR("WebsocketTLS initialize failed");
        return 1;
    }

    ws_tls_->setOnMessage([&](ClientHandler<server_tls> *client_hdl, const std::string &msg) {
        return onMessage(client_hdl, msg);
    });
    ws_tls_->setOnShutdown([&](ClientHandler<server_tls> *client_hdl) {

    });

    ws_ = std::make_shared<WSServer<server_plain>>();
    if (ws_->init())
    {
        ELOG_ERROR("Websocket initialize failed");
        return 1;
    }

    ws_->setOnMessage([&](ClientHandler<server_plain> *client_hdl, const std::string &msg) {
        return onMessage(client_hdl, msg);
    });
    ws_->setOnShutdown([&](ClientHandler<server_plain> *client_hdl) {

    });

    amqp_ = std::make_shared<AMQPRPC>();
    if (amqp_->init())
    {
        ELOG_ERROR("AMQP initialize failed");
        return 1;
    }

    amqp_signaling_ = std::make_shared<AMQPRecv>();
    if (amqp_signaling_->init([&](const std::string &msg) {
            onSignalingMessage(msg);
        }))
    {
        ELOG_ERROR("AMQPRecv initialize failed");
        return 1;
    }

    init_ = true;
    return 0;
}

void ErizoController::close()
{
    if (!init_)
        return;
    redis_->close();
    redis_.reset();
    redis_ = nullptr;

    ws_tls_->close();
    ws_tls_.reset();
    ws_tls_ = nullptr;

    ws_->close();
    ws_.reset();
    ws_ = nullptr;

    amqp_->close();
    amqp_.reset();
    amqp_ = nullptr;

    amqp_signaling_->close();
    amqp_signaling_.reset();
    amqp_signaling_ = nullptr;
    
    init_ = false;
}

void ErizoController::onSignalingMessage(const std::string &msg)
{
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(msg, root))
    {
        ELOG_ERROR("Signaling message parse failed");
        return;
    }

    if (!root.isMember("data") ||
        root["data"].type() != Json::objectValue)
    {
        ELOG_ERROR("Signaling message without data");
        return;
    }

    Json::Value data = root["data"];
    if (!data.isMember("type") ||
        data["type"].type() != Json::stringValue ||
        !data.isMember("streamId") ||
        data["streamId"].type() != Json::stringValue ||
        !data.isMember("clientId") ||
        data["clientId"].type() != Json::stringValue ||
        !data.isMember("erizoId") ||
        data["erizoId"].type() != Json::stringValue ||
        !data.isMember("agentId") ||
        data["agentId"].type() != Json::stringValue)
    {
        ELOG_ERROR("Signaling message data format error");
        return;
    }

    std::string agent_id = data["agentId"].asString();
    std::string erizo_id = data["erizoId"].asString();
    std::string client_id = data["clientId"].asString();
    std::string stream_id = data["streamId"].asString();
    std::string type = data["type"].asString();

    Json::FastWriter writer;
    std::vector<std::string> events;
    if (!type.compare("started"))
    {
        {
            Json::Value event;
            event[0] = "signaling_message_erizo";
            Json::Value mess;
            mess["agentId"] = agent_id;
            mess["erizoId"] = erizo_id;
            mess["type"] = "initializing";
            Json::Value event_data;
            event_data["streamId"] = stream_id;
            event_data["mess"] = mess;
            event[1] = event_data;
            events.push_back(writer.write(event));
        }
        {
            Json::Value event;
            event[0] = "signaling_message_erizo";
            Json::Value mess;
            mess["type"] = "started";
            Json::Value event_data;
            event_data["streamId"] = stream_id;
            event_data["mess"] = mess;
            event[1] = event_data;
            events.push_back(writer.write(event));
        }
    }
    else if (!type.compare("answer"))
    {
        if (!data.isMember("sdp") ||
            data["sdp"].type() != Json::stringValue)
        {
            ELOG_ERROR("Sdp answer format error");
            return;
        }

        Json::Value event;
        event[0] = "signaling_message_erizo";
        Json::Value mess;
        mess["type"] = "answer";
        mess["sdp"] = data["sdp"].asString();
        Json::Value event_data;
        event_data["streamId"] = stream_id;
        event_data["mess"] = mess;
        event[1] = event_data;
        events.push_back(writer.write(event));
    }

    std::shared_ptr<ClientHandler<server_plain>> plain_client_hdl = ws_->getClient(client_id);
    if (plain_client_hdl != nullptr)
    {
        for (const std::string &event : events)
            plain_client_hdl->sendEvent(event);
    }
    std::shared_ptr<ClientHandler<server_tls>> ssl_client_hdl = ws_tls_->getClient(client_id);
    if (ssl_client_hdl != nullptr)
    {
        for (const std::string &event : events)
            ssl_client_hdl->sendEvent(event);
    }
}

int ErizoController::getErizo(const std::string &agent_id, const std::string &room_id, std::string &erizo_id)
{
    std::string queuename = "ErizoAgent_" + agent_id;
    Json::Value data;
    data["method"] = "getErizo";
    data["roomID"] = room_id;

    int ret;
    std::atomic<bool> callback_done;
    int try_time = 3;
    do
    {
        try_time--;
        callback_done = false;
        amqp_->addRPC(Config::getInstance()->uniquecast_exchange_, queuename, queuename, data, [&](const Json::Value &root) {
            if (root.type() == Json::nullValue)
            {
                ret = 1;
                callback_done = true;
                return;
            }
            if (!root.isMember("erizo_id") ||
                root["erizo_id"].type() != Json::stringValue)
            {
                ret = 1;
                callback_done = true;
                return;
            }

            erizo_id = root["erizo_id"].asString();
            ret = 0;
            callback_done = true;
        });
        while (!callback_done)
            usleep(0);
    } while (ret && try_time);
    return ret;
}

int ErizoController::addPublisher(const std::string &erizo_id,
                                  const std::string &client_id,
                                  const std::string &stream_id,
                                  const std::string &label)
{
    std::string queuename = "Erizo_" + erizo_id;
    Json::Value data;
    data["method"] = "addPublisher";
    Json::Value args;
    args[0] = client_id;
    args[1] = stream_id;
    args[2] = label;
    args[3] = amqp_signaling_->getReplyTo();
    data["args"] = args;

    int ret;
    std::atomic<bool> callback_done;
    int try_time = 3;
    do
    {
        try_time--;
        callback_done = false;
        amqp_->addRPC(Config::getInstance()->uniquecast_exchange_, queuename, queuename, data, [&](const Json::Value &root) {
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

int ErizoController::processSignaling(const std::string &erizo_id,
                                      const std::string &client_id,
                                      const std::string &stream_id,
                                      const Json::Value &msg)
{
    std::string queuename = "Erizo_" + erizo_id;
    Json::Value data;
    data["method"] = "processSignaling";
    Json::Value args;
    args[0] = client_id;
    args[1] = stream_id;
    args[2] = msg;
    data["args"] = args;

    int ret;
    std::atomic<bool> callback_done;
    int try_time = 3;
    do
    {
        try_time--;
        callback_done = false;
        amqp_->addRPC(Config::getInstance()->uniquecast_exchange_, queuename, queuename, data, [&](const Json::Value &root) {
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

template <typename T>
void ErizoController::onShutdown(ClientHandler<T> *hdl)
{
}

template <typename T>
std::string ErizoController::onMessage(ClientHandler<T> *hdl, const std::string &msg)
{
    Client &client = hdl->getClient();
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(msg, root))
    {
        ELOG_ERROR("Message parse failed");
        return "shutdown";
    }

    if (root.type() != Json::arrayValue ||
        root.size() < 2 ||
        root[0].type() != Json::stringValue ||
        root[1].type() != Json::objectValue)
    {
        ELOG_ERROR("Event format error");
        return "shutdown";
    }
    Json::Value reply = Json::nullValue;
    std::string event = root[0].asString();
    Json::Value data = root[1];
    if (!event.compare("token"))
        reply = handleToken(client, data);
    else if (!event.compare("publish"))
        reply = handlePublish(client, data);
    else if (!event.compare("signaling_message"))
        reply = handleSignaling(client, data);

    if (reply != Json::nullValue)
    {
        if (reply.type() == Json::arrayValue && reply.size() == 0)
            return "notreply";

        Json::FastWriter writer;
        return writer.write(reply);
    }
    return "shutdown";
}

Json::Value ErizoController::handleToken(Client &client, const Json::Value &root)
{
    client.room_id = "test_room_id";
    client.agent_id = "1111111111";

    Json::Value data;
    data["id"] = client.room_id;
    data["clientId"] = client.id;
    data["streams"] = Json::arrayValue;
    data["singlePC"] = false;
    data["defaultVideoBW"] = 300;
    data["maxVideoBW"] = 300;
    Json::Value ice_data;
    ice_data["url"] = "stun:stun.l.google.com:19302";
    Json::Value ice;
    ice[0] = ice_data;
    data["iceServers"] = ice;
    Json::Value reply;
    reply[0] = "success";
    reply[1] = data;
    return reply;
}

Json::Value ErizoController::handlePublish(Client &client, const Json::Value &root)
{
    int ret;

    if (!root.isMember("label") ||
        root["label"].type() != Json::stringValue)
    {
        ELOG_ERROR("Publish data format error");
        return Json::nullValue;
    }

    ret = getErizo(client.agent_id, client.room_id, client.erizo_id);
    if (ret)
    {
        ELOG_ERROR("getErizo failed,client %s", client.id);
        return Json::nullValue;
    }

    Publisher publisher;
    publisher.id = Utils::getStreamID();
    publisher.erizo_id = client.erizo_id;
    publisher.agent_id = client.agent_id;
    client.publishers[publisher.id] = publisher;

    if (addPublisher(publisher.erizo_id, client.id, publisher.id, root["label"].asString()))
    {
        ELOG_ERROR("addPublisher failed,client %s,publisher %s", client.id, publisher.id);
        return Json::nullValue;
    }

    if (redis_->addPublisher(client.room_id, publisher))
    {
        ELOG_ERROR("Add publisher to redis failed");
        return Json::nullValue;
    }

    Json::Value reply;
    reply[0] = publisher.id;
    reply[1] = publisher.erizo_id;
    return reply;
}

Json::Value ErizoController::handleSignaling(Client &client, const Json::Value &root)
{
    if (!root.isMember("streamId") ||
        root["streamId"].type() != Json::stringValue ||
        !root.isMember("msg") ||
        root["msg"].type() != Json::objectValue)
    {
        ELOG_ERROR("Signaling message data format error");
        return Json::nullValue;
    }

    std::string erizo_id = client.erizo_id;
    std::string client_id = client.id;
    std::string stream_id = root["streamId"].asString();

    if (processSignaling(erizo_id, client_id, stream_id, root["msg"]))
    {
        ELOG_ERROR("processSignaling failed,client %s,stream %s", client_id, stream_id);
        return Json::nullValue;
    }

    return Json::arrayValue;
}