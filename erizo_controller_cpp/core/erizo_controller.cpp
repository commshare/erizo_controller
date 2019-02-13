#include "erizo_controller.h"

#include "redis/redis_helper.h"
#include "redis/redis_locker.h"
#include "rabbitmq/amqp_rpc.h"
#include "rabbitmq/amqp_recv.h"
#include "common/utils.h"
#include "common/config.h"
#include "websocket/socket_io_server.h"
#include "websocket/socket_io_client_handler.h"
#include "thread/thread_pool.h"

DEFINE_LOGGER(ErizoController, "ErizoController");

ErizoController *ErizoController::instance_ = nullptr;
ErizoController *ErizoController::getInstance()
{
    if (!instance_)
        instance_ = new ErizoController;
    return instance_;
}

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

    thread_pool_ = std::unique_ptr<erizo::ThreadPool>(new erizo::ThreadPool(Config::getInstance()->erizo_controller_worker_num));
    thread_pool_->start();

    amqp_ = std::make_shared<AMQPRPC>();
    if (amqp_->init())
    {
        ELOG_ERROR("amqp initialize failed");
        return 1;
    }

    amqp_signaling_ = std::make_shared<AMQPRecv>();
    if (amqp_signaling_->initUniquecast([this](const std::string &msg) {
            onSignalingMessage(msg);
        }))
    {
        ELOG_ERROR("amqp-signaling initialize failed");
        return 1;
    }

    socket_io_ = std::make_shared<SocketIOServer>();
    if (socket_io_->init())
    {
        ELOG_ERROR("socket-io-server initialize failed");
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
    auto it = Config::getInstance()->server_mapping.find((int)client.ip_info.area);
    if (it == Config::getInstance()->server_mapping.end())
    {
        ELOG_ERROR("not server deploy on field %d", (int)client.ip_info.area);
        return 1;
    }

    std::string name = it->second;
    if (RedisHelper::getAllErizoAgent(name, agents))
    {
        ELOG_ERROR("getall erizo-agent from redis failed");
        return 1;
    }

    uint64_t now = Utils::getSystemMs();
    std::vector<ErizoAgent> agents_alive;
    for (const ErizoAgent &agent : agents)
    {
        if (now - agent.last_update < (uint64_t)Config::getInstance()->erizo_agent_timeout)
            agents_alive.push_back(agent);
    }

    if (agents_alive.size() == 0)
    {
        ELOG_ERROR("not erizo-agent alive on field %d", (int)client.ip_info.area);
        return 1;
    }

    std::sort(agents_alive.begin(), agents_alive.end(), [](ErizoAgent &a, ErizoAgent &b) {
        return a.erizo_process_num < b.erizo_process_num;
    });
    client.agent_id = agents_alive[0].id;
    return 0;
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
        amqp_->rpc(Config::getInstance()->uniquecast_exchange, queuename, queuename, data, [this, &client, &ret, &callback_done](const Json::Value &root) {
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
        Json::Reader reader(Json::Features::strictMode());
        if (!reader.parse(msg, root))
        {
            ELOG_ERROR("json parse root failed,dump %s", msg);
            return;
        }
        if (!root.isMember("data") ||
            root["data"].type() != Json::objectValue)
        {
            ELOG_ERROR("json parse data failed,dump %s", msg);
            return;
        }
        Json::Value data = root["data"];
        if (!data.isMember("type") || data["type"].type() != Json::stringValue ||
            !data.isMember("clientId") || data["clientId"].type() != Json::stringValue)
        {
            ELOG_ERROR("json parse [type/clientId] failed,dump %s", msg);
            return;
        }
        std::string type = data["type"].asString();
        std::string client_id = data["clientId"].asString();

        Json::FastWriter writer;
        std::vector<std::string> events;
        if (type == "started")
        {
            if (!data.isMember("agentId") || data["agentId"].type() != Json::stringValue ||
                !data.isMember("erizoId") || data["erizoId"].type() != Json::stringValue ||
                !data.isMember("streamId") || data["streamId"].type() != Json::stringValue)
            {
                ELOG_ERROR("json parse [agentId/erizoId/streamId] failed,dump %s", msg);
                return;
            }
            std::string agent_id = data["agentId"].asString();
            std::string erizo_id = data["erizoId"].asString();
            std::string stream_id = data["streamId"].asString();
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
        else if (type == "publisher_answer")
        {

            if (!data.isMember("streamId") || data["streamId"].type() != Json::stringValue ||
                !data.isMember("sdp") || data["sdp"].type() != Json::stringValue ||
                !data.isMember("roomId") || data["roomId"].type() != Json::stringValue ||
                !data.isMember("videoSSRC") || !data.isMember("audioSSRC"))
            {
                ELOG_ERROR("json parse [streamId/sdp/roomId/videoSSRC/audioSSRC] failed,dump %s", msg);
                return;
            }
            std::string room_id = data["roomId"].asString();
            std::string stream_id = data["streamId"].asString();
            std::string sdp = data["sdp"].asString();
            uint32_t video_ssrc = data["videoSSRC"].asUInt();
            uint32_t audio_ssrc = data["audioSSRC"].asUInt();

            RedisLocker redis_locker;
            if (!redis_locker.lock(room_id))
            {
                ELOG_ERROR("get redis locker failed when publisher-answer");
                return;
            }

            Publisher publisher;
            if (RedisHelper::getPublisher(room_id, stream_id, publisher))
            {
                ELOG_ERROR("get publisher from redis failed");
                return;
            }
            publisher.video_ssrc = video_ssrc;
            publisher.audio_ssrc = audio_ssrc;
            if (RedisHelper::addPublisher(room_id, publisher))
            {
                ELOG_ERROR("add publisher to redis failed");
                return;
            }

            Json::Value event;
            event[0] = "signaling_message_erizo";
            Json::Value mess;
            mess["type"] = "answer";
            mess["sdp"] = sdp;
            Json::Value event_data;
            event_data["streamId"] = stream_id;
            event_data["mess"] = mess;
            event[1] = event_data;
            events.push_back(writer.write(event));
        }
        else if (type == "subscriber_answer")
        {
            if (!data.isMember("streamId") || data["streamId"].type() != Json::stringValue ||
                !data.isMember("sdp") || data["sdp"].type() != Json::stringValue ||
                !data.isMember("erizoId") || data["erizoId"].type() != Json::stringValue)
            {
                ELOG_ERROR("json parse [streamId/sdp/erizoId] failed,dump %s", msg);
                return;
            }
            std::string sdp = data["sdp"].asString();
            std::string erizo_id = data["erizoId"].asString();
            std::string stream_id = data["streamId"].asString();

            Json::Value event;
            event[0] = "signaling_message_erizo";
            Json::Value mess;
            mess["type"] = "answer";
            mess["sdp"] = sdp;
            mess["erizoId"] = erizo_id;
            Json::Value event_data;
            event_data["peerId"] = stream_id;
            event_data["mess"] = mess;
            event[1] = event_data;
            events.push_back(writer.write(event));
        }
        else if (type == "ready")
        {
            if (!data.isMember("streamId") || data["streamId"].type() != Json::stringValue)
            {
                ELOG_ERROR("json parse streamId failed,dump %s", msg);
                return;
            }
            std::string stream_id = data["streamId"].asString();

            if (data.isMember("roomId") || data["roomId"].type() == Json::stringValue)
            {
                std::string room_id = data["roomId"].asString();
                RedisLocker redis_locker;
                if (!redis_locker.lock(room_id))
                {
                    ELOG_ERROR("get redis locker failed when ready");
                    return;
                }
                notifyToSubscribe(room_id, client_id, stream_id);
            }
        }
        else if (type == "new_publisher")
        {
            if (!data.isMember("label") || data["label"].type() != Json::stringValue ||
                !data.isMember("streamId") || data["streamId"].type() != Json::stringValue)
            {
                ELOG_ERROR("json parse [label/streamId] failed,dump %s", msg);
                return;
            }
            std::string label = data["label"].asString();
            std::string stream_id = data["streamId"].asString();

            Json::Value event;
            event[0] = "onAddStream";
            Json::Value event_data;
            event_data["id"] = stream_id;
            event_data["audio"] = true;
            event_data["video"] = true;
            event_data["data"] = true;
            event_data["label"] = label;
            event_data["screen"] = Json::stringValue;
            event[1] = event_data;
            events.push_back(writer.write(event));
        }
        else if (type == "remove_subscriber")
        {
            if (!data.isMember("streamId") || data["streamId"].type() != Json::stringValue)
            {
                ELOG_ERROR("json parse streamId failed,dump %s", msg);
                return;
            }
            std::string stream_id = data["streamId"].asString();

            Json::Value event;
            event[0] = "onRemoveStream";
            Json::Value event_data;
            event_data["id"] = stream_id;
            event[1] = event_data;
            events.push_back(writer.write(event));
        }
        else if (type == "notifyErizoProcessQuit")
        {
            socket_io_->closeConnection(client_id);
            return;
        }

        for (const std::string &event : events)
            socket_io_->sendEvent(client_id, event);
    });
}

void ErizoController::removePublisher(const Publisher &publisher)
{
    std::string queuename = publisher.erizo_id;
    Json::Value data;
    data["method"] = "removePublisher";
    Json::Value args;
    args[0] = publisher.client_id;
    args[1] = publisher.id;
    data["args"] = args;
    amqp_->rpcNotReply(queuename, data);
}

void ErizoController::removeSubscriber(const Subscriber &subscriber)
{
    std::string queuename = subscriber.erizo_id;
    Json::Value data;
    data["method"] = "removeSubscriber";
    Json::Value args;
    args[0] = subscriber.client_id;
    args[1] = subscriber.subscribe_to;
    data["args"] = args;
    amqp_->rpcNotReply(queuename, data);
}

void ErizoController::notifyToSubscribe(const std::string &room_id, const std::string &client_id, const std::string &stream_id)
{
    asyncTask([=]() {
        Publisher publisher;
        if (RedisHelper::getPublisher(room_id, stream_id, publisher))
        {
            ELOG_ERROR("get publisher from redis failed");
            return;
        }

        std::vector<Client> clients;
        if (RedisHelper::getAllClient(room_id, clients))
        {
            ELOG_ERROR("getall client from redis failed");
            return;
        }

        for (const Client &client : clients)
        {
            if (client.id == client_id)
                continue;
            Json::Value root;
            root["type"] = "new_publisher";
            root["streamId"] = stream_id;
            root["clientId"] = client.id;
            root["label"] = publisher.label;
            amqp_->rpcNotReply(client.reply_to, root);
        }
    });
}

void ErizoController::addPublisher(const Client &client, const Publisher &publisher)
{
    std::string queuename = publisher.erizo_id;
    Json::Value data;
    data["method"] = "addPublisher";
    Json::Value args;
    args[0] = client.room_id;
    args[1] = publisher.client_id;
    args[2] = publisher.id;
    args[3] = publisher.label;
    args[4] = amqp_signaling_->getReplyTo();
    data["args"] = args;
    amqp_->rpcNotReply(queuename, data);
}

void ErizoController::addVirtualPublisher(const Publisher &publisher, const BridgeStream &bridge_stream)
{
    std::string queuename = bridge_stream.recver_erizo_id;
    Json::Value data;
    data["method"] = "addVirtualPublisher";
    Json::Value args;
    args[0] = bridge_stream.id;            // bridge_stream id
    args[1] = bridge_stream.src_stream_id; // src_stream_id
    args[2] = bridge_stream.sender_ip;
    args[3] = bridge_stream.sender_port;
    args[4] = publisher.video_ssrc;
    args[5] = publisher.audio_ssrc;
    data["args"] = args;
    amqp_->rpcNotReply(queuename, data);
}

void ErizoController::addSubscriber(const Publisher &publisher, const Subscriber &subscriber)
{
    std::string queuename = subscriber.erizo_id;
    Json::Value data;
    data["method"] = "addSubscriber";
    Json::Value args;
    args[0] = subscriber.client_id;
    args[1] = subscriber.subscribe_to;
    args[2] = publisher.label;
    args[3] = amqp_signaling_->getReplyTo();
    data["args"] = args;
    amqp_->rpcNotReply(queuename, data);
}

void ErizoController::addVirtualSubscriber(const BridgeStream &bridge_stream)
{
    std::string queuename = bridge_stream.sender_erizo_id;
    Json::Value data;
    data["method"] = "addVirtualSubscriber";
    Json::Value args;
    args[0] = bridge_stream.id;            // bridge_stream id
    args[1] = bridge_stream.src_stream_id; // src_stream_id
    args[2] = bridge_stream.recver_ip;
    args[3] = bridge_stream.recver_port;
    data["args"] = args;
    amqp_->rpcNotReply(queuename, data);
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
    amqp_->rpcNotReply(queuename, data);
}

void ErizoController::notifyToRemoveSubscriber(const Subscriber &subscriber)
{
    Json::Value data;
    data["type"] = "remove_subscriber";
    data["streamId"] = subscriber.subscribe_to;
    data["clientId"] = subscriber.client_id;
    amqp_->rpcNotReply(subscriber.reply_to, data);
}

void ErizoController::onClose(SocketIOClientHandler *hdl)
{
    Client &client = hdl->getClient();
    std::vector<Subscriber> subscribers;

    RedisLocker redis_locker;
    if (!redis_locker.lock(client.room_id))
    {
        ELOG_ERROR("get redis locker failed when on-close");
        return;
    }
    if (RedisHelper::getAllSubscriber(client.room_id, subscribers))
    {
        ELOG_ERROR("getall subscriber from redis failed");
        return;
    }

    std::vector<Publisher> publishers;
    if (RedisHelper::getAllPublisher(client.room_id, publishers))
    {
        ELOG_ERROR("getall publisher from redis failed");
        return;
    }

    std::vector<std::string> subscribers_to_del;
    for (const Subscriber &subscriber : subscribers)
    {
        for (const Publisher &publisher : publishers)
        {
            //删除订阅其他客户端订阅此客户端的流
            if (publisher.client_id == client.id && subscriber.subscribe_to == publisher.id)
            {
                subscribers_to_del.push_back(subscriber.id);
                if (subscriber.is_bridge && removeBridgeStreamSub(client.room_id, subscriber.subscribe_to))
                {
                    ELOG_ERROR("remove bridge-stream-sub on redis failed");
                    return;
                }
                removeSubscriber(subscriber);
                notifyToRemoveSubscriber(subscriber);
            }
        }
        //删除此客户端订阅的流
        if (subscriber.client_id == client.id)
        {
            subscribers_to_del.push_back(subscriber.id);
            if (subscriber.is_bridge && removeBridgeStreamSub(client.room_id, subscriber.subscribe_to))
            {
                ELOG_ERROR(" remove bridge-stream-sub on redis failed");
                return;
            }
            removeSubscriber(subscriber);
            notifyToRemoveSubscriber(subscriber);
        }
    }

    std::vector<std::string> publishers_to_del;
    for (const Publisher &publisher : publishers)
    {
        //删除此客户端推送的流
        if (publisher.client_id == client.id)
        {
            publishers_to_del.push_back(publisher.id);
            if (removeBridgeStreamPub(client.room_id, publisher.id))
            {
                ELOG_ERROR("remove bridge-stream-pub on redis failed");
                return;
            }
            removePublisher(publisher);
        }
    }

    if (!subscribers_to_del.empty())
        RedisHelper::removeSubscribers(client.room_id, subscribers_to_del);

    if (!publishers_to_del.empty())
        RedisHelper::removePublishers(client.room_id, publishers_to_del);

    RedisHelper::removeClient(client.room_id, client.id);
}

std::string ErizoController::onMessage(SocketIOClientHandler *hdl, const std::string &msg)
{
    Client &client = hdl->getClient();
    Json::Value root;
    Json::Reader reader(Json::Features::strictMode());
    if (!reader.parse(msg, root))
    {
        ELOG_ERROR("json parse root failed,dump %s", msg);
        return "disconnect";
    }

    if (root.type() != Json::arrayValue ||
        root.size() < 2 ||
        root[0].type() != Json::stringValue ||
        root[1].type() != Json::objectValue)
    {
        ELOG_ERROR("json parse args[num/type] failed,dump %s", msg);
        return "disconnect";
    }
    Json::Value reply = Json::nullValue;
    std::string event = root[0].asString();
    Json::Value data = root[1];
    if (event == "token")
        reply = handleToken(client, data);
    else if (event == "publish")
        reply = handlePublish(client, data);
    else if (event == "subscribe")
    {
        reply = handleSubscribe(client, data);
        if (reply == Json::nullValue)
            return "keep";
    }
    else if (event == "signaling_message")
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
        ELOG_ERROR("add client to redis failed");
        return Json::nullValue;
    }

    std::vector<Publisher> publishers;
    if (RedisHelper::getAllPublisher(client.room_id, publishers))
    {
        ELOG_ERROR("getall publisher from redis failed");
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
        ELOG_ERROR("get redis locker failed when handle-publish");
        return Json::nullValue;
    }

    if (RedisHelper::addPublisher(client.room_id, publisher))
    {
        ELOG_ERROR("add publisher to redis failed");
        return Json::nullValue;
    }

    addPublisher(client, publisher);

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
        ELOG_ERROR("json parse streamId failed,dump %s", Utils::dumpJson(root));
        return Json::nullValue;
    }

    int try_time = 10;
again:
    RedisLocker redis_locker;
    if (!redis_locker.lock(client.room_id))
    {
        ELOG_ERROR("get redis locker failed when handle-subscribe");
        return Json::nullValue;
    }

    std::string stream_id = root["streamId"].asString();
    Publisher publisher;
    if (RedisHelper::getPublisher(client.room_id, stream_id, publisher))
    {
        ELOG_ERROR("get publisher from redis failed");
        return Json::nullValue;
    }

    if (publisher.video_ssrc == 0 || publisher.audio_ssrc == 0)
    {
        if (try_time--)
        {
            usleep(100000); //100ms
            goto again;
        }
        return Json::nullValue;
    }

    bool is_bridge = !(client.agent_id == publisher.agent_id);
    Subscriber subscriber;
    subscriber.id = Utils::getStreamID();
    subscriber.client_id = client.id;
    subscriber.erizo_id = client.erizo_id;
    subscriber.agent_id = client.agent_id;
    subscriber.subscribe_to = stream_id;
    subscriber.reply_to = amqp_signaling_->getReplyTo();
    subscriber.is_bridge = is_bridge;

    if (RedisHelper::addSubscriber(client.room_id, subscriber))
    {
        ELOG_ERROR("add subscriber to redis failed");
        return Json::nullValue;
    }

    if (is_bridge)
    {
        std::vector<BridgeStream> bridge_streams;
        if (RedisHelper::getAllBridgeStream(client.room_id, bridge_streams))
        {
            redis_locker.unlock();
            ELOG_ERROR("getall bridge-stream from redis failed");
            return Json::nullValue;
        }

        auto it = std::find_if(bridge_streams.begin(), bridge_streams.end(), [&stream_id](const BridgeStream &bridge_stream) {
            if (bridge_stream.src_stream_id == stream_id)
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
            bridge_stream.recver_erizo_id = client.erizo_id;
            bridge_stream.recver_ip = client.bridge_ip;
            bridge_stream.recver_port = client.bridge_port;
            bridge_stream.src_stream_id = stream_id;
            bridge_stream.subscribe_count = 1;

            Publisher publisher;
            if (RedisHelper::getPublisher(client.room_id, bridge_stream.src_stream_id, publisher))
            {
                ELOG_ERROR("get publisher from redis failed");
                return Json::nullValue;
            }

            if (RedisHelper::addBridgeStream(client.room_id, bridge_stream))
            {
                ELOG_ERROR("add bridge-stream to redis failed");
                return Json::nullValue;
            }

            addVirtualPublisher(publisher, bridge_stream);
            addVirtualSubscriber(bridge_stream);
        }
        else
        {
            BridgeStream bridge_stream = *it;
            bridge_stream.subscribe_count++;
            if (RedisHelper::addBridgeStream(client.room_id, bridge_stream))
            {
                ELOG_ERROR("add bridge-stream to redis failed");
                return Json::nullValue;
            }
        }
    }

    addSubscriber(publisher, subscriber);

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
        ELOG_ERROR("json parse [streamId/msg] failed,dump %s", Utils::dumpJson(root));
        return;
    }

    std::string erizo_id = client.erizo_id;
    std::string client_id = client.id;
    std::string stream_id = root["streamId"].asString();
    processSignaling(erizo_id, client_id, stream_id, root["msg"]);
}

void ErizoController::removeVirtualPublisher(const BridgeStream &bridge_stream)
{
    std::string queuename = bridge_stream.recver_erizo_id;
    Json::Value data;
    data["method"] = "removeVirtualPublisher";
    Json::Value args;
    args[0] = bridge_stream.src_stream_id;
    data["args"] = args;
    amqp_->rpcNotReply(queuename, data);
}

void ErizoController::removeVirtualSubscriber(const BridgeStream &bridge_stream)
{
    std::string queuename = bridge_stream.sender_erizo_id;
    Json::Value data;
    data["method"] = "removeVirtualSubscriber";
    Json::Value args;
    args[0] = bridge_stream.id;
    args[1] = bridge_stream.src_stream_id;
    data["args"] = args;
    amqp_->rpcNotReply(queuename, data);
}

int ErizoController::removeBridgeStreamSub(const std::string &room_id, const std::string &subscribe_to)
{
    std::vector<BridgeStream> bridge_streams;
    if (RedisHelper::getAllBridgeStream(room_id, bridge_streams))
    {
        ELOG_ERROR("getall bridge-stream from redis failed");
        return 1;
    }

    auto it = std::find_if(bridge_streams.begin(), bridge_streams.end(), [subscribe_to](const BridgeStream &s) {
        if (s.src_stream_id == subscribe_to)
            return true;
        return false;
    });

    if (it != bridge_streams.end())
    {
        BridgeStream bridge_stream = *it;
        bridge_stream.subscribe_count--;
        if (bridge_stream.subscribe_count <= 0)
        {
            removeVirtualSubscriber(bridge_stream);
            removeVirtualPublisher(bridge_stream);

            if (RedisHelper::removeBridgeStream(room_id, bridge_stream.id))
            {
                ELOG_ERROR("remove bridge-stream on redis failed");
                return 1;
            }
        }
        else
        {
            if (RedisHelper::addBridgeStream(room_id, bridge_stream))
            {
                ELOG_ERROR("add bridge-stream to redis failed");
                return 1;
            }
        }
    }
    return 0;
}

int ErizoController::removeBridgeStreamPub(const std::string &room_id, const std::string &stream_id)
{
    std::vector<BridgeStream> bridge_streams;
    if (RedisHelper::getAllBridgeStream(room_id, bridge_streams))
    {
        ELOG_ERROR("getall bridge-stream from redis failed");
        return 1;
    }

    auto it = std::find_if(bridge_streams.begin(), bridge_streams.end(), [stream_id](const BridgeStream &s) {
        if (s.src_stream_id == stream_id)
            return true;
        return false;
    });

    if (it != bridge_streams.end())
    {
        BridgeStream bridge_stream = *it;
        removeVirtualSubscriber(bridge_stream);
        removeVirtualPublisher(bridge_stream);
        if (RedisHelper::removeBridgeStream(room_id, bridge_stream.id))
        {
            ELOG_ERROR("remove bridge-stream on redis failed");
            return 1;
        }
    }
    return 0;
}
