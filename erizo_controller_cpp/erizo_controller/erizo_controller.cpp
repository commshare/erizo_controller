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
    ws_tls_->setEventCallback([this](server_tls *server, websocketpp::connection_hdl hdl, const std::string &msg) {
        std::string reply;
        if (handleEvent(msg, reply))
            server->send(hdl, reply, websocketpp::frame::opcode::TEXT);
    });

    ws_ = std::make_shared<WSServer<server_plain>>();
    if (ws_->init())
    {
        ELOG_ERROR("Websocket initialize failed");
        return 1;
    }
    ws_->setEventCallback([this](server_plain *server, websocketpp::connection_hdl hdl, const std::string &msg) {
        std::string reply;
        if (handleEvent(msg, reply))
            server->send(hdl, reply, websocketpp::frame::opcode::TEXT);
    });

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

bool ErizoController::handleEvent(const std::string &msg, std::string &reply)
{
    int mid;
    bool has_mid = false;

    int pos = msg.find_first_of('[');
    if (pos > 2)
    {
        has_mid = true;
        mid = std::stoi(msg.substr(2, pos));
    }

    std::string event = msg.substr(pos);

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(event, root))
    {
        ELOG_ERROR("Event parse failed");
        return false;
    }

    if (root.isNull() ||
        root.type() != Json::arrayValue ||
        root.size() != 2 ||
        root[0].type() != Json::stringValue ||
        root[1].type() != Json::objectValue)
    {
        ELOG_ERROR("Event format error");
        return false;
    }

    std::ostringstream oss;
    std::string reply_data = "";

    std::string type = root[0].asString();
    if (!type.compare("token"))
    {
        reply_data = handleToken(root[1]);
    }

    oss << "43";
    if (has_mid)
        oss << mid;
    oss << reply_data;
    reply = oss.str();

    return true;
}

std::string ErizoController::handleToken(const Json::Value &root)
{
    std::string client_id = Utils::getUUID();
    Json::Value data;
    data["id"] = "00000"; //room id
    data["clientId"] = client_id;
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

    redisclient::RedisValue res = redis_->command("RPUSH", {"00000", client_id});
    if (res.isOk())
    {
        res = redis_->command("LRANGE", {"00000", "0", "-1"});
        if (res.isOk() && res.isArray())
            std::vector<redisclient::RedisValue> arr = res.getArray();
    }
    Json::FastWriter writer;
    return writer.write(reply);
}
