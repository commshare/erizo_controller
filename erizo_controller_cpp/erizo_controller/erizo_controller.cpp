#include "erizo_controller.h"

DEFINE_LOGGER(ErizoController, "ErizoController");

ErizoController::ErizoController() : redis_(nullptr),
                                     ws_tls_(nullptr),
                                     ws_(nullptr)
{
}

ErizoController::~ErizoController()
{
}

int ErizoController::init()
{
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
        std::string reply_msg;
        daptch(client_hdl, msg, reply_msg);
        return reply_msg;
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
        std::string reply_msg;
        daptch(client_hdl, msg, reply_msg);
        return reply_msg;
    });
    ws_->setOnShutdown([&](ClientHandler<server_plain> *client_hdl) {

    });

    amqp_ = std::make_shared<AMQPRPC>();
    if (amqp_->init())
    {
        ELOG_ERROR("AMQP initialize failed");
        return 1;
    }

    amqp_boardcast_ = std::make_shared<AMQPRPCBoardcast>();
    if (amqp_boardcast_->init([&](const std::string &msg) {
            Json::Value root;
            Json::Reader reader;
            if (!reader.parse(msg, root))
            {
                ELOG_ERROR("Boardcast message parse failed");
                return;
            }

            Json::Value data = root["data"];
            if (!root.isMember("data") ||
                data.type() != Json::objectValue ||
                !data.isMember("method") ||
                data["method"].type() != Json::stringValue)
            {
                ELOG_ERROR("Boardcast message with not method");
                return;
            }

            std::string method = data["method"].asString();
            if (!method.compare("getErizoAgents"))
            {
                getErizoAgents(root);
            }
        }))
    {
        ELOG_ERROR("AMQPBoardcast initialize failed");
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

    run_ = true;
    keeplive_thread_ = std::unique_ptr<std::thread>(new std::thread([&]() {
        while (run_)
        {
            Json::Value msg;
            msg["method"] = "getErizoAgents";

            amqp_boardcast_->addRPC("ErizoAgent",
                                    "ErizoAgent",
                                    msg);

            {
                std::unique_lock<std::mutex>(agents_map_mux_);
                for (auto it = agents_map_.begin(); it != agents_map_.end();)
                {
                    it->second.timeout++;
                    if (it->second.timeout > 3)
                    {
                        ELOG_INFO("EA: id-->%s die", it->second.id);
                        it = agents_map_.erase(it);
                        continue;
                    }
                    it++;
                }
            }
            usleep(500000); //500ms
        }
    }));

    return 0;
}

void ErizoController::close()
{
    amqp_->close();
    amqp_.reset();
    amqp_ = nullptr;

    amqp_boardcast_->close();
    amqp_boardcast_.reset();
    amqp_boardcast_ = nullptr;

    amqp_signaling_->close();
    amqp_signaling_.reset();
    amqp_signaling_ = nullptr;

    ws_->close();
    ws_.reset();
    ws_ = nullptr;

    ws_tls_->close();
    ws_tls_.reset();
    ws_tls_ = nullptr;

    redis_->close();
    redis_.reset();
    redis_ = nullptr;
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
        data["streamId"].type() != Json::stringValue)
    {
        ELOG_ERROR("Signaling message data format error");
        return;
    }

    std::string stream_id = data["streamId"].asString();
    std::string type = data["type"].asString();

    Stream stream;
    {
        std::unique_lock<std::mutex> lock(streams_map_mux_);
        if (streams_map_.find(stream_id) == streams_map_.end())
        {
            ELOG_ERROR("Stream %s not exist", stream_id);
            return;
        }
        stream = streams_map_[stream_id];
    }

    Json::FastWriter writer;
    std::vector<std::string> events;
    if (!type.compare("started"))
    {
        {
            Json::Value event;
            event[0] = "signaling_message_erizo";
            Json::Value mess;
            mess["agentId"] = stream.agent_id;
            mess["erizoId"] = stream.erizo_id;
            mess["type"] = "initializing";
            Json::Value event_data;
            event_data["streamId"] = stream.id;
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
            event_data["streamId"] = stream.id;
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
            ELOG_ERROR("Wrong sdp answer");
            return;
        }

        Json::Value event;
        event[0] = "signaling_message_erizo";
        Json::Value mess;
        mess["type"] = "answer";
        mess["sdp"] = data["sdp"].asString();
        Json::Value event_data;
        event_data["streamId"] = stream.id;
        event_data["mess"] = mess;
        event[1] = event_data;
        events.push_back(writer.write(event));
    }

    if (stream.plain_client_hdl != nullptr)
    {
        ClientHandler<server_plain> *hdl = (ClientHandler<server_plain> *)stream.plain_client_hdl;
        for (const std::string &event : events)
            hdl->sendEvent(event);
    }
    else if (stream.ssl_client_hdl != nullptr)
    {
        ClientHandler<server_tls> *hdl = (ClientHandler<server_tls> *)stream.ssl_client_hdl;
        for (const std::string &event : events)
            hdl->sendEvent(event);
    }
    else
    {
        ELOG_ERROR("Client handler is null");
        return;
    }
}

void ErizoController::getErizo(const std::string &agent_id, const std::string &room_id, const std::function<void(const Json::Value &)> &func)
{
    std::string queuename = "ErizoAgent_" + agent_id;
    Json::Value data;
    data["method"] = "getErizo";
    data["roomID"] = room_id;
    amqp_->addRPC(Config::getInstance()->uniquecast_exchange_,
                  queuename,
                  queuename,
                  data,
                  func);
}

void ErizoController::processSignaling(const std::string &erizo_id,
                                       const std::string &client_id,
                                       const std::string &stream_id,
                                       const Json::Value &msg,
                                       const std::function<void(const Json::Value &)> &func)
{
    std::string queuename = "Erizo_" + erizo_id;
    Json::Value data;
    data["method"] = "processSignaling";
    Json::Value args;
    args[0] = client_id;
    args[1] = stream_id;
    args[2] = msg;
    data["args"] = args;
    amqp_->addRPC(Config::getInstance()->uniquecast_exchange_,
                  queuename,
                  queuename,
                  data,
                  func);
}

void ErizoController::addPublisher(const std::string &erizo_id,
                                   const std::string &client_id,
                                   const std::string &stream_id,
                                   const std::string &label,
                                   const std::function<void(const Json::Value &)> &func)
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
    amqp_->addRPC(Config::getInstance()->uniquecast_exchange_,
                  queuename,
                  queuename,
                  data,
                  func);
}

void ErizoController::getErizoAgents(const Json::Value &root)
{
    Json::Value data = root["data"];
    if (!root.isMember("data") ||
        data.type() != Json::objectValue ||
        !data.isMember("id") ||
        data["id"].type() != Json::stringValue ||
        !data.isMember("ip") ||
        data["ip"].type() != Json::stringValue)
    {
        ELOG_ERROR("Message format error");
        return;
    }

    std::string id = data["id"].asString();
    std::string ip = data["ip"].asString();
    std::unique_lock<std::mutex> lock(agents_map_mux_);
    agents_map_[id] = {id, ip, 0};
}

template <typename T>
int ErizoController::daptch(ClientHandler<T> *hdl, const std::string &msg, std::string &reply_msg)
{
    Client client;
    {
        std::unique_lock<std::mutex> lock(clients_map_mux_);
        auto it = clients_map_.find((void *)hdl);
        if (it == clients_map_.end())
        {
            std::string ip;
            uint16_t port;
            hdl->getAddress(ip, port);

            client.id = Utils::getUUID();
            client.ip = ip;
            client.port = port;

            printf("########### hdl:%#x\n", hdl);
            if (std::is_same<T, server_plain>())
            {
                client.plain_client_hdl = hdl;
            }
            else
            {
                client.ssl_client_hdl = hdl;
            }

            clients_map_[(void *)hdl] = client;
            ELOG_INFO("New client id:%s", client.id);
        }
        else
        {
            client = it->second;
        }
    }

    reply_msg = "shutdown";
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(msg, root))
    {
        ELOG_ERROR("Message parse failed");
        return 1;
    }

    if (root.type() != Json::arrayValue ||
        root.size() < 2 ||
        root[0].type() != Json::stringValue ||
        root[1].type() != Json::objectValue)
    {
        ELOG_ERROR("Event format error");
        return 1;
    }

    Json::Value reply = Json::nullValue;
    std::string event = root[0].asString();
    Json::Value data = root[1];
    if (!event.compare("token"))
    {
        reply = handleToken(client, data);
    }
    else if (!event.compare("publish"))
    {
        reply = handlePublish(client, data);
    }
    else if (!event.compare("signaling_message"))
    {
        handleSignaling(client, data);
        reply_msg = "notreply";
    }

    if (reply != Json::nullValue)
    {
        Json::FastWriter writer;
        reply_msg = writer.write(reply);
    }

    {
        std::unique_lock<std::mutex> lock(clients_map_mux_);
        clients_map_[(void *)hdl] = client;
    }

    return 0;
}

Json::Value ErizoController::handleToken(Client &client, const Json::Value &root)
{
    //#############################################################################
    //TODO 根据client的ip分配agent,agent列表从redis获得
    {
        client.room_id = "test_room_id";
        std::unique_lock<std::mutex> lock(agents_map_mux_);
        auto it = agents_map_.begin();
        for (; it != agents_map_.end(); it++)
        {
            client.agent_id = it->second.id;
            client.agent_ip = it->second.ip;
            break;
        }

        if (it == agents_map_.end())
        {
            ELOG_WARN("Not agent online");
            return Json::nullValue;
        }
    }
    //#############################################################################

    Json::Value data;
    data["id"] = client.room_id; //room id
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
    std::atomic<bool> callback_done(false);

    if (!root.isMember("label") ||
        root["label"].type() != Json::stringValue)
    {
        ELOG_ERROR("Publish data format error");
        return Json::nullValue;
    }

    std::string label = root["label"].asString();

    getErizo(client.agent_id, client.room_id, [&](const Json::Value &root) {
        if (root.type() == Json::nullValue)
        {
            ret = 1;
            callback_done = true;
            ELOG_ERROR("getErizo timeout");
            return;
        }
        if (!root.isMember("erizo_id") ||
            root["erizo_id"].type() != Json::stringValue)
        {
            ret = 1;
            callback_done = true;
            ELOG_ERROR("getErizo data format error");
            return;
        }
        client.erizo_id = root["erizo_id"].asString();
        ret = 0;
        callback_done = true;
        ELOG_INFO("Client get erizo %s", client.erizo_id);
    });

    while (!callback_done)
        usleep(0);

    if (ret)
    {
        ELOG_ERROR("Client %s get erizo failed", client.id);
        return Json::nullValue;
    }

    Publisher publisher;
    publisher.id = Utils::getStreamID();
    publisher.erizo_id = client.erizo_id;
    publisher.agent_id = client.agent_id;
    publisher.plain_client_hdl = client.plain_client_hdl;
    publisher.ssl_client_hdl = client.ssl_client_hdl;
    {
        std::unique_lock<std::mutex>(streams_map_mux_);
        streams_map_[publisher.id] = publisher;
    }

    callback_done = false;
    addPublisher(publisher.erizo_id, client.id, publisher.id, label, [&](const Json::Value &root) {
        if (root.type() == Json::nullValue)
        {
            ret = 1;
            callback_done = true;
            ELOG_ERROR("addPublisher timeout");
            return;
        }

        if (!root.isMember("ret") ||
            root["ret"].type() != Json::intValue)
        {
            ret = 1;
            callback_done = true;
            ELOG_ERROR("Erizo reply message error");
            return;
        }

        ret = root["ret"].asInt();
        callback_done = true;
    });

    while (!callback_done)
        usleep(0);

    if (ret)
    {
        ELOG_ERROR("Client %s addPublisher failed", client.id);
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

void ErizoController::handleSignaling(Client &client, const Json::Value &root)
{
    if (!root.isMember("streamId") ||
        root["streamId"].type() != Json::stringValue ||
        !root.isMember("msg") ||
        root["msg"].type() != Json::objectValue)
    {
        ELOG_ERROR("Signaling data format error");
        return;
    }

    processSignaling(client.erizo_id, client.id, root["streamId"].asString(), root["msg"], [&](const Json::Value &root) {
        if (!root.isMember("ret") ||
            root["ret"].type() != Json::intValue)
        {
            ELOG_ERROR("############# failed");
            return;
        }
        int ret = root["ret"].asInt();
        if (ret != 0)
        {
            ELOG_ERROR("error occ");
        }
    });
}