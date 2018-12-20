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
    ws_tls_->setOnMessage([this](ClientHandler<server_tls> *client_hdl, const std::string &msg) {
        // std::string ip;
        // uint16_t port;
        // client_hdl->getAddress(ip, port);

        // std::string reply;
        // if (handleEvent(ip, port, msg, reply))
        // {
        //     ELOG_ERROR("@@@@@@@@@@@@ error @@@@@@@@@@@@@@");
        //     return;
        // }
        // server->send(hdl, reply, websocketpp::frame::opcode::TEXT);
        return "";
    });

    ws_ = std::make_shared<WSServer<server_plain>>();
    if (ws_->init())
    {
        ELOG_ERROR("Websocket initialize failed");
        return 1;
    }
    ws_->setOnMessage([this](ClientHandler<server_plain> *client_hdl, const std::string &msg) {
        // if (handleEvent(ip, port, msg, reply))
        // {

        //     return;
        // }
        ELOG_ERROR("%s", msg);
        return "";
    });
    ws_->setOnShutdown([this](ClientHandler<server_plain> *client_hdl) {
        std::string ip;
        uint16_t port;
        client_hdl->getAddress(ip,port);
   
    });

    amqp_ = std::make_shared<AMQPRPC>();
    if (amqp_->init("rpcExchange", "direct"))
    {
        ELOG_ERROR("AMQP initialize failed");
        return 1;
    }

    amqp_boardcast_ = std::make_shared<AMQPRPCBoardcast>();
    if (amqp_boardcast_->init("rpcExchange", "topic", [&, this](const std::string &msg) {
            Json::Value root;
            Json::Reader reader;
            if (!reader.parse(msg, root))
            {
                ELOG_ERROR("Boardcast message parse failed");
                return;
            }

            Json::Value data = root["data"];
            if (data.isNull() ||
                data.type() != Json::objectValue ||
                data["method"].isNull() ||
                data["method"].type() != Json::stringValue)
            {
                ELOG_ERROR("Boardcast message with not method");
                return;
            }

            std::string method = data["method"].asString();
            if (!method.compare("getErizoAgents"))
            {
                //    getErizoAgents(root);
            }
        }))
    {
        ELOG_ERROR("AMQPBoardcast initialize failed");
        return 1;
    }

    run_ = true;
    keeplive_thread_ = std::unique_ptr<std::thread>(new std::thread([&, this]() {
        while (run_)
        {
            Json::Value msg;
            msg["method"] = "getErizoAgents";

            amqp_boardcast_->addRPC("broadcastExchange",
                                    "ErizoAgent",
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
            //      getErizo("");
            usleep(500000); //500ms
        }
    }));

    return 0;
}

void ErizoController::close()
{
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

// int ErizoController::handleEvent(std::string ip, uint16_t port, const std::string &msg, std::string &reply)
// {

//     std::string event = msg.substr(pos);

//     Json::Value root;
//     Json::Reader reader;
//     if (!reader.parse(event, root))
//     {
//         ELOG_ERROR("Event parse failed");
//         return 1;
//     }

//     if (root.isNull() ||
//         root.type() != Json::arrayValue ||
//         root.size() != 2 ||
//         root[0].type() != Json::stringValue ||
//         root[1].type() != Json::objectValue)
//     {
//         ELOG_ERROR("Event format error");
//         return 1;
//     }

//     std::string type = root[0].asString();
//     if (!type.compare("token"))
//     {
//         reply_data = handleToken(root[1]);
//     }
//     return 0;
// }

// std::string ErizoController::handleToken(const Json::Value &root)
// {
//     std::string client_id = Utils::getUUID();
//     Json::Value data;
//     data["id"] = "00000"; //room id
//     data["clientId"] = client_id;
//     data["streams"] = Json::arrayValue;
//     data["singlePC"] = false;
//     data["defaultVideoBW"] = 300;
//     data["maxVideoBW"] = 300;

//     Json::Value ice_data;
//     ice_data["url"] = "stun:stun.l.google.com:19302";

//     Json::Value ice;
//     ice[0] = ice_data;

//     data["iceServers"] = ice;

//     Json::Value reply;
//     reply[0] = "success";
//     reply[1] = data;

//     redisclient::RedisValue res = redis_->command("RPUSH", {"00000", client_id});
//     if (res.isOk())
//     {
//         res = redis_->command("LRANGE", {"00000", "0", "-1"});
//         if (res.isOk() && res.isArray())
//             std::vector<redisclient::RedisValue> arr = res.getArray();
//     }
//     Json::FastWriter writer;
//     return writer.write(reply);
// }

// void ErizoController::getErizoAgents(const Json::Value &root)
// {
//     Json::Value data = root["data"];
//     if (data.isNull() ||
//         data.type() != Json::objectValue ||
//         data["id"].isNull() ||
//         data["id"].type() != Json::stringValue ||
//         data["ip"].isNull() ||
//         data["ip"].type() != Json::stringValue)
//     {
//         ELOG_ERROR("Message format error");
//         return;
//     }

//     std::string id = data["id"].asString();
//     std::string ip = data["ip"].asString();
//     std::unique_lock<std::mutex> lock(agents_map_mux_);
//     agents_map_[id] = {id, ip, 0};
// }

// //unimplement method
// ErizoAgent ErizoController::allocateAgent()
// {
//     return {};
// }

// void ErizoController::getErizo(std::string room_id)
// {
//     //  ErizoAgent agent = allocateAgent();
//     std::unique_lock<std::mutex> lock(agents_map_mux_);
//     for (auto it = agents_map_.begin(); it != agents_map_.end(); it++)
//     {
//         std::string queuename = "ErizoAgent_" + it->second.id;

//         Json::Value data;
//         data["roomID"] = "testestestest";
//         data["method"] = "getErizo";
//         amqp_->addRPC("rpcExchange",
//                       queuename,
//                       queuename,
//                       data,
//                       [&, this](const Json::Value &root) {
//                           printf("############################\n");
//                       });
//     }
// }