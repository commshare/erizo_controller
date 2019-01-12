#include "erizo_controller.h"

DEFINE_LOGGER(ErizoController, "ErizoController");

ErizoController::ErizoController() : redis_(nullptr),
                                     socket_io_(nullptr),
                                     amqp_(nullptr),
                                     amqp_signaling_(nullptr),
                                     thread_pool_(nullptr),
                                     init_(false)
{
}

ErizoController::~ErizoController()
{
}

void ErizoController::asyncTask(const std::function<void()> &func)
{
    std::shared_ptr<erizo::Worker> worker = thread_pool_->getLessUsedWorker();
    worker->task(func);
}

int ErizoController::init()
{
    if (init_)
        return 0;

    thread_pool_ = std::unique_ptr<erizo::ThreadPool>(new erizo::ThreadPool(Config::getInstance()->worker_num_));
    thread_pool_->start();

    redis_ = std::make_shared<RedisHelper>();
    if (redis_->init())
    {
        ELOG_ERROR("Redis initialize failed");
        return 1;
    }

    socket_io_ = std::make_shared<SocketIOServer>();
    if (socket_io_->init())
    {
        ELOG_ERROR("SocketIO server initialize failed");
        return 1;
    }
    socket_io_->onMessage([this](SocketIOClientHandler *hdl, const std::string &msg) {
        return onMessage(hdl, msg);
    });
    socket_io_->onClose([this](SocketIOClientHandler *hdl) {
        onClose(hdl);
    });

    amqp_ = std::make_shared<AMQPRPC>();
    if (amqp_->init())
    {
        ELOG_ERROR("AMQP initialize failed");
        return 1;
    }

    amqp_signaling_ = std::make_shared<AMQPRecv>();
    if (amqp_signaling_->init([this](const std::string &msg) {
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

    socket_io_->close();
    socket_io_.reset();
    socket_io_ = nullptr;

    amqp_->close();
    amqp_.reset();
    amqp_ = nullptr;

    amqp_signaling_->close();
    amqp_signaling_.reset();
    amqp_signaling_ = nullptr;

    thread_pool_->close();
    thread_pool_.reset();
    thread_pool_ = nullptr;

    init_ = false;
}

int ErizoController::allocAgent(Client &client)
{
    std::vector<ErizoAgent> agents;
    if (client.ip_info.isp == edu::iptable::ISPType::AUTO_DETECT && client.ip_info.area == edu::iptable::AreaType::AREA_UNKNOWN)
    {
        if (redis_->getAllErizoAgent(Config::getInstance()->default_area_, agents))
        {
            ELOG_ERROR("getAllErizoAgent failed");
            return 1;
        }

        uint64_t now = Utils::getCurrentMs();
        std::vector<ErizoAgent> agents_alive;
        for (const ErizoAgent &agent : agents)
        {
            if (now - agent.last_update < (uint64_t)Config::getInstance()->erizo_agent_timeout_)
                agents_alive.push_back(agent);
        }

        if (agents_alive.size() == 0)
        {
            ELOG_ERROR("not erizo agent alive");
            return 1;
        }

        std::sort(agents_alive.begin(), agents_alive.end(), [](ErizoAgent &a, ErizoAgent &b) {
            return a.erizo_process_num < b.erizo_process_num;
        });

        client.agent_id = agents_alive[0].id;
        return 0;
    }
    return 1;
}

int ErizoController::allocErizo(Client &client)
{
    Json::Value data;
    data["method"] = "getErizo";
    data["roomID"] = client.room_id;
    std::string queuename = client.agent_id;

    int ret;
    std::atomic<bool> callback_done;
    int try_time = 3;

    do
    {
        try_time--;
        callback_done = false;
        amqp_->addRPC(Config::getInstance()->uniquecast_exchange_, queuename, queuename, data, [this, &client, &ret, &callback_done](const Json::Value &root) {
            if (root.type() == Json::nullValue)
            {
                ret = 1;
                callback_done = true;
                return;
            }
            if (!root.isMember("erizoID") || root["erizoID"].type() != Json::stringValue)
            {
                ret = 1;
                callback_done = true;
                return;
            }

            ret = 0;
            client.erizo_id = root["erizoID"].asString();
            callback_done = true;
        });
        while (!callback_done)
            usleep(0);
    } while (ret && try_time);
    return ret;
}

void ErizoController::onSignalingMessage(const std::string &msg)
{
    asyncTask([=]() {
        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(msg, root))
            return;

        if (!root.isMember("data") ||
            root["data"].type() != Json::objectValue)
            return;

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
            return;

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
        else if (!type.compare("publisher_answer"))
        {
            if (!data.isMember("sdp") ||
                data["sdp"].type() != Json::stringValue)
                return;

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
        else if (!type.compare("subscriber_answer"))
        {
            if (!data.isMember("sdp") ||
                data["sdp"].type() != Json::stringValue)
                return;

            Json::Value event;
            event[0] = "signaling_message_erizo";
            Json::Value mess;
            mess["type"] = "answer";
            mess["sdp"] = data["sdp"].asString();
            mess["erizoId"] = erizo_id;
            Json::Value event_data;
            event_data["peerId"] = stream_id;
            event_data["mess"] = mess;
            event[1] = event_data;
            events.push_back(writer.write(event));
        }
        else if (!type.compare("ready"))
        {
            notifyToSubscribe(client_id, stream_id);
        }
        else if (!type.compare("new_publisher"))
        {
            if (!data.isMember("label") ||
                data["label"].type() != Json::stringValue)
                return;

            Json::Value event;
            event[0] = "onAddStream";
            Json::Value event_data;
            event_data["id"] = stream_id;
            event_data["audio"] = true;
            event_data["video"] = true;
            event_data["data"] = true;
            event_data["label"] = data["label"].asString();
            event_data["screen"] = Json::stringValue;
            event[1] = event_data;
            events.push_back(writer.write(event));
        }
        else if (!type.compare("remove_subscriber"))
        {
            removeSubscriber(erizo_id, client_id, stream_id);
            Json::Value event;
            event[0] = "onRemoveStream";
            Json::Value event_data;
            event_data["id"] = stream_id;
            event[1] = event_data;
            events.push_back(writer.write(event));
        }

        for (const std::string &event : events)
            socket_io_->sendEvent(client_id, event);
    });
}

int ErizoController::rpc(const std::string &queuename, const Json::Value &data)
{
    int ret;
    std::atomic<bool> callback_done;
    int try_time = 3;
    do
    {
        try_time--;
        callback_done = false;
        amqp_->addRPC(Config::getInstance()->uniquecast_exchange_, queuename, queuename, data, [this, &ret, &callback_done](const Json::Value &root) {
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

void ErizoController::removePublisher(const std::string &erizo_id, const std::string &client_id, const std::string &stream_id)
{
    std::string queuename = erizo_id;
    Json::Value data;
    data["method"] = "removePublisher";
    Json::Value args;
    args[0] = client_id;
    args[1] = stream_id;
    data["args"] = args;
    rpc(queuename, data);
}

void ErizoController::removeSubscriber(const std::string &erizo_id, const std::string &client_id, const std::string &stream_id)
{
    std::string queuename = erizo_id;
    Json::Value data;
    data["method"] = "removeSubscriber";
    Json::Value args;
    args[0] = client_id;
    args[1] = stream_id;
    data["args"] = args;
    rpc(queuename, data);
}

void ErizoController::notifyToSubscribe(const std::string &client_id, const std::string &stream_id)
{
    asyncTask([=]() {
        std::string room_id;
        if (redis_->getRoomByClientId(client_id, room_id))
        {
            ELOG_ERROR("Redis getRoomByClientId failed");
            return;
        }

        Publisher publisher;
        if (redis_->getPublisher(room_id, stream_id, publisher))
        {
            ELOG_ERROR("Redis getPublisher failed");
        }

        std::vector<Client> clients;
        if (redis_->getAllClient(room_id, clients))
        {
            ELOG_ERROR("Redis getAllClient failed");
            return;
        }

        for (const Client &client : clients)
        {
            if (!client.id.compare(client_id))
                continue;
            Json::Value root;
            root["type"] = "new_publisher";
            root["streamId"] = stream_id;
            root["clientId"] = client.id;
            root["erizoId"] = client.erizo_id;
            root["agentId"] = client.agent_id;
            root["label"] = publisher.label;
            amqp_->sendMessage(Config::getInstance()->uniquecast_exchange_,
                               client.reply_to,
                               client.reply_to,
                               root);
        }
    });
}

int ErizoController::addPublisher(const std::string &erizo_id,
                                  const std::string &client_id,
                                  const std::string &stream_id,
                                  const std::string &label)
{
    std::string queuename = erizo_id;
    Json::Value data;
    data["method"] = "addPublisher";
    Json::Value args;
    args[0] = client_id;
    args[1] = stream_id;
    args[2] = label;
    args[3] = amqp_signaling_->getReplyTo();
    data["args"] = args;
    return rpc(queuename, data);
}

int ErizoController::addSubscriber(const std::string &erizo_id,
                                   const std::string &client_id,
                                   const std::string &stream_id,
                                   const std::string &label)
{
    std::string queuename = erizo_id;
    Json::Value data;
    data["method"] = "addSubscriber";
    Json::Value args;
    args[0] = client_id;
    args[1] = stream_id;
    args[2] = label;
    args[3] = amqp_signaling_->getReplyTo();
    data["args"] = args;
    return rpc(queuename, data);
}

void ErizoController::processSignaling(const std::string &erizo_id,
                                       const std::string &client_id,
                                       const std::string &stream_id,
                                       const Json::Value &msg)
{
    std::string queuename = erizo_id;
    Json::Value data;
    data["method"] = "processSignaling";
    Json::Value args;
    args[0] = client_id;
    args[1] = stream_id;
    args[2] = msg;
    data["args"] = args;
    amqp_->sendMessage(Config::getInstance()->uniquecast_exchange_,
                       queuename,
                       queuename,
                       data);
}

void ErizoController::onClose(SocketIOClientHandler *hdl)
{
    Client &client = hdl->getClient();
    std::vector<Subscriber> subscribers;

    if (redis_->getAllSubscriber(client.room_id, subscribers))
    {
        ELOG_ERROR("Redis getAllSubscriber failed");
        return;
    }

    std::vector<Publisher> publishers;
    if (redis_->getAllPublisher(client.room_id, publishers))
    {
        ELOG_ERROR("Redis getAllPublisher failed");
        return;
    }

    std::vector<std::string> subscribers_to_del;
    for (const Subscriber &subscriber : subscribers)
    {
        for (const Publisher &publisher : publishers)
        {
            if (!publisher.client_id.compare(client.id) && !subscriber.subscribe_to.compare(publisher.id))
            {
                subscribers_to_del.push_back(subscriber.id);
                Json::Value root;
                root["type"] = "remove_subscriber";
                root["streamId"] = subscriber.subscribe_to;
                root["clientId"] = subscriber.client_id;
                root["erizoId"] = subscriber.erizo_id;
                root["agentId"] = subscriber.agent_id;
                amqp_->sendMessage(Config::getInstance()->uniquecast_exchange_,
                                   subscriber.reply_to,
                                   subscriber.reply_to,
                                   root);
            }
        }

        if (!subscriber.client_id.compare(client.id))
        {
            subscribers_to_del.push_back(subscriber.id);
            Json::Value root;
            root["type"] = "remove_subscriber";
            root["streamId"] = subscriber.subscribe_to;
            root["clientId"] = subscriber.client_id;
            root["erizoId"] = subscriber.erizo_id;
            root["agentId"] = subscriber.agent_id;
            amqp_->sendMessage(Config::getInstance()->uniquecast_exchange_,
                               subscriber.reply_to,
                               subscriber.reply_to,
                               root);
        }
    }

    std::vector<std::string> publishers_to_del;
    for (const Publisher &publisher : publishers)
    {
        if (!publisher.client_id.compare(client.id))
        {
            publishers_to_del.push_back(publisher.id);
            removePublisher(publisher.erizo_id, publisher.client_id, publisher.id);
        }
    }

    if (!subscribers_to_del.empty() && redis_->removeSubscribers(client.room_id, subscribers_to_del))
    {
        ELOG_ERROR("Redis removeSubscriber failed");
        return;
    }

    if (!publishers_to_del.empty() && redis_->removePublishers(client.room_id, publishers_to_del))
    {
        ELOG_ERROR("Redis removePublisher failed");
        return;
    }

    if (redis_->removeClientRoomMapping(client.id))
    {
        ELOG_ERROR("Redis removeClientRoomMapping failed");
        return;
    }

    if (redis_->removeClient(client.room_id, client.id))
    {
        ELOG_ERROR("Redis removeClient failed");
        return;
    }
}

std::string ErizoController::onMessage(SocketIOClientHandler *hdl, const std::string &msg)
{
    Client &client = hdl->getClient();
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(msg, root))
    {
        ELOG_ERROR("Message parse failed");
        return "disconnect";
    }

    if (root.type() != Json::arrayValue ||
        root.size() < 2 ||
        root[0].type() != Json::stringValue ||
        root[1].type() != Json::objectValue)
    {
        ELOG_ERROR("Event format error");
        return "disconnect";
    }
    Json::Value reply = Json::nullValue;
    std::string event = root[0].asString();
    Json::Value data = root[1];
    if (!event.compare("token"))
        reply = handleToken(client, data);
    else if (!event.compare("publish"))
        reply = handlePublish(client, data);
    else if (!event.compare("subscribe"))
    {
        reply = handleSubscribe(client, data);
        if (reply == Json::nullValue)
            return "keep";
    }
    else if (!event.compare("signaling_message"))
    {
        handleSignaling(client, data);
        return "keep";
    }
    if (reply != Json::nullValue)
    {
        Json::FastWriter writer;
        return writer.write(reply);
    }
    return "disconnect";
}

Json::Value ErizoController::handleToken(Client &client, const Json::Value &root)
{
    client.room_id = "test_room_id";
    client.reply_to = amqp_signaling_->getReplyTo();

    if (allocAgent(client))
        return Json::nullValue;

    if (allocErizo(client))
        return Json::nullValue;

    if (redis_->addClient(client.room_id, client))
    {
        ELOG_ERROR("addClient failed");
        return Json::nullValue;
    }

    if (redis_->addClientRoomMapping(client.id, client.room_id))
    {
        ELOG_ERROR("addClientRoomMapping failed");
        return Json::nullValue;
    }

    std::vector<Publisher> publishers;
    if (redis_->getAllPublisher(client.room_id, publishers))
    {
        ELOG_ERROR("getAllPublisher failed");
        return Json::nullValue;
    }

    Json::Value data;
    for (const Publisher &publisher : publishers)
    {
        Json::Value temp;
        temp["id"] = publisher.id;
        temp["audio"] = true;
        temp["video"] = true;
        temp["data"] = true;
        temp["label"] = publisher.label;
        temp["screen"] = Json::stringValue;
        data["streams"].append(temp);
    }
    data["id"] = client.room_id;
    data["clientId"] = client.id;
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
    if (!root.isMember("label") ||
        root["label"].type() != Json::stringValue)
    {
        ELOG_ERROR("Publish data format error");
        return Json::nullValue;
    }

    std::string label = root["label"].asString();
    Publisher publisher;
    publisher.id = Utils::getStreamID();
    publisher.erizo_id = client.erizo_id;
    publisher.agent_id = client.agent_id;
    publisher.client_id = client.id;
    publisher.label = label;

    if (redis_->addPublisher(client.room_id, publisher))
    {
        ELOG_ERROR("Add publisher to redis failed");
        return Json::nullValue;
    }

    if (addPublisher(publisher.erizo_id, client.id, publisher.id, label))
    {
        ELOG_ERROR("addPublisher failed,client %s,publisher %s", client.id, publisher.id);
        return Json::nullValue;
    }

    Json::Value reply;
    reply[0] = publisher.id;
    reply[1] = publisher.erizo_id;
    return reply;
}

Json::Value ErizoController::handleSubscribe(Client &client, const Json::Value &root)
{
    if (!root.isMember("streamId") ||
        root["streamId"].type() != Json::stringValue)
    {
        ELOG_ERROR("Publish data format error");
        return Json::nullValue;
    }

    std::string stream_id = root["streamId"].asString();
    Publisher publisher;
    if (redis_->getPublisher(client.room_id, stream_id, publisher))
    {
        ELOG_ERROR("Publisher not exist");
        return Json::nullValue;
    }

    Subscriber subscriber;
    subscriber.id = Utils::getUUID();
    subscriber.client_id = client.id;
    subscriber.erizo_id = client.erizo_id;
    subscriber.agent_id = client.agent_id;
    subscriber.subscribe_to = stream_id;
    subscriber.reply_to = amqp_signaling_->getReplyTo();

    if (redis_->addSubscriber(client.room_id, subscriber))
    {
        ELOG_ERROR("Add Subscriber to redis failed");
        return Json::nullValue;
    }

    if (addSubscriber(client.erizo_id, client.id, stream_id, publisher.label))
    {
        ELOG_ERROR("addSubscriber failed");
        return Json::nullValue;
    }

    Json::Value reply;
    reply[0] = true;
    reply[1] = subscriber.erizo_id;
    return reply;
}

void ErizoController::handleSignaling(Client &client, const Json::Value &root)
{
    if (!root.isMember("streamId") ||
        root["streamId"].type() != Json::stringValue ||
        !root.isMember("msg") ||
        root["msg"].type() != Json::objectValue)
    {
        ELOG_ERROR("Signaling message data format error");
        return;
    }

    std::string erizo_id = client.erizo_id;
    std::string client_id = client.id;
    std::string stream_id = root["streamId"].asString();
    processSignaling(erizo_id, client_id, stream_id, root["msg"]);
}
