#include "erizo_controller.h"

DEFINE_LOGGER(ErizoController, "ErizoController");

ErizoController::ErizoController() : socket_io_(nullptr),
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

    init_ = true;
    return 0;
}

void ErizoController::close()
{
    if (!init_)
        return;

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

int testtest = 1;
int ErizoController::allocAgent(Client &client)
{
    std::vector<ErizoAgent> agents;
    if (client.ip_info.isp == edu::iptable::ISPType::AUTO_DETECT && client.ip_info.area == edu::iptable::AreaType::AREA_UNKNOWN)
    {
        if (RedisHelper::getAllErizoAgent(Config::getInstance()->default_area_, agents))
        {
            ELOG_ERROR("getAllErizoAgent failed");
            return 1;
        }

        uint64_t now = Utils::getSystemMs();
        std::vector<ErizoAgent> agents_alive;
        for (const ErizoAgent &agent : agents)
        {
            if (now - agent.last_update < (uint64_t)Config::getInstance()->erizo_agent_timeout_)
                agents_alive.push_back(agent);
        }

        // if (agents_alive.size() == 0)
        // {
        //     ELOG_ERROR("not erizo agent alive");
        //     return 1;
        // }
        if (agents_alive.size() < 2)
        {
            ELOG_ERROR("not erizo agent alive");
            return 1;
        }

        // std::sort(agents_alive.begin(), agents_alive.end(), [](ErizoAgent &a, ErizoAgent &b) {
        //     return a.erizo_process_num < b.erizo_process_num;
        // });
        if (testtest % 2 == 0)
        {
            client.agent_id = agents_alive[0].id;
        }
        else
        {
            client.agent_id = agents_alive[1].id;
        }
        testtest++;
        // client.agent_id = agents_alive[0].id;
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
        amqp_->rpc(Config::getInstance()->uniquecast_exchange_, queuename, queuename, data, [this, &client, &ret, &callback_done](const Json::Value &root) {
            if (root.type() == Json::nullValue)
            {
                ret = 1;
                callback_done = true;
                return;
            }
            if (!root.isMember("erizoID") ||
                root["erizoID"].type() != Json::stringValue ||
                !root.isMember("bridgeIP") ||
                root["bridgeIP"].type() != Json::stringValue ||
                !root.isMember("bridgePort") ||
                root["bridgePort"].type() != Json::intValue)
            {
                ret = 1;
                callback_done = true;
                return;
            }

            ret = 0;
            client.erizo_id = root["erizoID"].asString();
            client.bridge_ip = root["bridgeIP"].asString();
            client.bridge_port = root["bridgePort"].asInt();

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
                data["sdp"].type() != Json::stringValue ||
                !data.isMember("roomId") ||
                data["roomId"].type() != Json::stringValue ||
                !data.isMember("video_ssrc") ||
                !data.isMember("audio_ssrc"))
                return;

            std::string room_id = data["roomId"].asString();
            uint32_t video_ssrc = data["video_ssrc"].asUInt();
            uint32_t audio_ssrc = data["audio_ssrc"].asUInt();

            RedisLocker redis_locker;
            if (!redis_locker.lock(room_id))
            {
                ELOG_ERROR("publisher_answer get redis locker failed");
                return;
            }

            Publisher publisher;
            if (RedisHelper::getPublisher(room_id, stream_id, publisher))
            {
                ELOG_ERROR("redis getPublisher failed");
                return;
            }
            publisher.video_ssrc = video_ssrc;
            publisher.audio_ssrc = audio_ssrc;
            if (RedisHelper::addPublisher(room_id, publisher))
            {
                ELOG_ERROR("redis addPublisher failed");
                return;
            }

            redis_locker.unlock();

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
            if (!data.isMember("roomId") ||
                data["roomId"].type() != Json::stringValue)
                return;
            std::string room_id = data["roomId"].asString();

            RedisLocker redis_locker;
            if (!redis_locker.lock(room_id))
            {
                ELOG_ERROR("notifyToSubscribe get redis locker failed");
                return;
            }
            notifyToSubscribe(room_id, client_id, stream_id);
            redis_locker.unlock();
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

void ErizoController::removePublisher(const std::string &erizo_id, const std::string &client_id, const std::string &stream_id)
{
    std::string queuename = erizo_id;
    Json::Value data;
    data["method"] = "removePublisher";
    Json::Value args;
    args[0] = client_id;
    args[1] = stream_id;
    data["args"] = args;
    amqp_->rpcNotReply(queuename, data);
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
    amqp_->rpcNotReply(queuename, data);
}

void ErizoController::notifyToSubscribe(const std::string &room_id, const std::string &client_id, const std::string &stream_id)
{
    asyncTask([=]() {
        Publisher publisher;
        if (RedisHelper::getPublisher(room_id, stream_id, publisher))
        {
            ELOG_ERROR("Redis getPublisher failed");
        }

        std::vector<Client> clients;
        if (RedisHelper::getAllClient(room_id, clients))
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
                                  const std::string &room_id,
                                  const std::string &client_id,
                                  const std::string &stream_id,
                                  const std::string &label)
{
    std::string queuename = erizo_id;
    Json::Value data;
    data["method"] = "addPublisher";
    Json::Value args;
    args[0] = room_id;
    args[1] = client_id;
    args[2] = stream_id;
    args[3] = label;
    args[4] = amqp_signaling_->getReplyTo();
    data["args"] = args;
    return amqp_->rpc(queuename, data);
}

int ErizoController::addVirtualPublisher(const BridgeStream &bridge_stream)
{
    Publisher publisher;
    if (RedisHelper::getPublisher(bridge_stream.room_id, bridge_stream.src_stream_id, publisher))
    {
        ELOG_ERROR("redis getPublisher failed");
        return 1;
    }

    std::string queuename = bridge_stream.recver_erizo_id;
    Json::Value data;
    data["method"] = "addVirtualPublisher";
    Json::Value args;
    args[0] = bridge_stream.id;            // bridge_stream id
    args[1] = bridge_stream.src_stream_id; //src_stream_id
    args[2] = bridge_stream.sender_ip;
    args[3] = bridge_stream.sender_port;
    args[4] = publisher.video_ssrc;
    args[5] = publisher.audio_ssrc;
    data["args"] = args;
    return amqp_->rpc(queuename, data);
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
    return amqp_->rpc(queuename, data);
}

int ErizoController::addVirtualSubscriber(const BridgeStream &bridge_stream)
{
    std::string queuename = bridge_stream.sender_erizo_id;
    Json::Value data;
    data["method"] = "addVirtualSubscriber";
    Json::Value args;
    args[0] = bridge_stream.id;            // bridge_stream id
    args[1] = bridge_stream.src_stream_id; //src_stream_id
    args[2] = bridge_stream.recver_ip;
    args[3] = bridge_stream.recver_port;
    data["args"] = args;
    return amqp_->rpc(queuename, data);
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

    RedisLocker redis_locker;
    if (!redis_locker.lock(client.room_id))
    {
        ELOG_ERROR("onClose get redis locker failed");
        return;
    }
    if (RedisHelper::getAllSubscriber(client.room_id, subscribers))
    {
        ELOG_ERROR("Redis getAllSubscriber failed");
        return;
    }

    std::vector<Publisher> publishers;
    if (RedisHelper::getAllPublisher(client.room_id, publishers))
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

    if (!subscribers_to_del.empty())
        RedisHelper::removeSubscribers(client.room_id, subscribers_to_del);

    if (!publishers_to_del.empty())
        RedisHelper::removePublishers(client.room_id, publishers_to_del);

    RedisHelper::removeClient(client.room_id, client.id);
    redis_locker.unlock();
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

    if (RedisHelper::addClient(client.room_id, client))
    {
        ELOG_ERROR("addClient failed");
        return Json::nullValue;
    }

    std::vector<Publisher> publishers;
    if (RedisHelper::getAllPublisher(client.room_id, publishers))
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
    publisher.bridge_ip = client.bridge_ip;
    publisher.bridge_port = client.bridge_port;
    publisher.agent_id = client.agent_id;
    publisher.client_id = client.id;
    publisher.label = label;

    RedisLocker redis_locker;
    if (!redis_locker.lock(client.room_id))
    {
        ELOG_ERROR("handlePublish get redis locker failed");
        return Json::nullValue;
    }

    if (RedisHelper::addPublisher(client.room_id, publisher))
    {
        ELOG_ERROR("Add publisher to redis failed");
        return Json::nullValue;
    }

    if (addPublisher(publisher.erizo_id, client.room_id, client.id, publisher.id, label))
    {
        ELOG_ERROR("addPublisher failed,client %s,publisher %s", client.id, publisher.id);
        return Json::nullValue;
    }

    redis_locker.unlock();

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
        ELOG_ERROR("Subscribe data format error");
        return Json::nullValue;
    }

    RedisLocker redis_locker;
    if (!redis_locker.lock(client.room_id))
    {
        ELOG_ERROR("handleSubscribe get redis locker failed");
        return Json::nullValue;
    }

    std::string stream_id = root["streamId"].asString();
    Publisher publisher;
    if (RedisHelper::getPublisher(client.room_id, stream_id, publisher))
    {
        ELOG_ERROR("Publisher not exist");
        return Json::nullValue;
    }

    Subscriber subscriber;
    subscriber.id = Utils::getStreamID();
    subscriber.client_id = client.id;
    subscriber.erizo_id = client.erizo_id;
    subscriber.bridge_ip = client.bridge_ip;
    subscriber.bridge_port = client.bridge_port;
    subscriber.agent_id = client.agent_id;
    subscriber.subscribe_to = stream_id;
    subscriber.reply_to = amqp_signaling_->getReplyTo();

    if (RedisHelper::addSubscriber(client.room_id, subscriber))
    {
        ELOG_ERROR("Add Subscriber to redis failed");
        return Json::nullValue;
    }

    if (subscriber.agent_id.compare(publisher.agent_id) != 0)
    {
        std::vector<BridgeStream> bridge_streams;
        if (RedisHelper::getAllBridgeStream(client.room_id, bridge_streams))
        {
            ELOG_ERROR("get all bridge from redis stream failed");
            return Json::nullValue;
        }

        auto it = std::find_if(bridge_streams.begin(), bridge_streams.end(), [&stream_id](const BridgeStream &bridge_stream) {
            if (!bridge_stream.src_stream_id.compare(stream_id))
                return true;
            return false;
        });
        if (it == bridge_streams.end())
        {
            BridgeStream bridge_stream;
            bridge_stream.id = Utils::getStreamID();
            bridge_stream.sender_erizo_id = publisher.erizo_id;
            bridge_stream.sender_ip = publisher.bridge_ip;
            bridge_stream.sender_port = publisher.bridge_port;
            bridge_stream.recver_erizo_id = subscriber.erizo_id;
            bridge_stream.recver_ip = subscriber.bridge_ip;
            bridge_stream.recver_port = subscriber.bridge_port;
            bridge_stream.src_stream_id = stream_id;
            bridge_stream.room_id = client.room_id;

            if (RedisHelper::addBridgeStream(client.room_id, bridge_stream))
            {
                ELOG_ERROR("add bridge stream to redis failed");
                return Json::nullValue;
            }

            if (addVirtualPublisher(bridge_stream))
            {
                ELOG_ERROR("add virtual publisher failed");
                return Json::nullValue;
            }

            if (addVirtualSubscriber(bridge_stream))
            {
                ELOG_ERROR("add virtual subscriber failed");
                return Json::nullValue;
            }
        }
    }

    if (addSubscriber(client.erizo_id, client.id, stream_id, publisher.label))
    {
        ELOG_ERROR("addSubscriber failed");
        return Json::nullValue;
    }

    redis_locker.unlock();

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
