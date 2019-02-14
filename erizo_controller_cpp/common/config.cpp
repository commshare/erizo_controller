#include "config.h"

#include <fstream>

DEFINE_LOGGER(Config, "Config");

Config *Config::instance_ = nullptr;

Config::Config()
{
    port = 8080;
    ssl_key = "cert/key.pem";
    ssl_cert = "cert/cert.pem";
    ssl_port = 443;

    mysql_url = "tcp://119.28.70.47:3306";
    mysql_username = "root";
    mysql_passwd = "cathy978";

    redis_ip = "127.0.0.1";
    redis_port = 6379;
    redis_passwd = "cathy978";
    redis_conn_timeout = 10;
    redis_rw_timeout = 10;
    redis_max_conns = 100;
    redis_lock_timeout = 2000; //ms
    redis_lock_try_time = 1000;

    rabbitmq_username = "linmin";
    rabbitmq_passwd = "linmin";
    rabbitmq_hostname = "127.0.0.1";
    rabbitmq_port = 5672;
    rabbitmq_timeout = 1000;
    uniquecast_exchange = "erizo_uniquecast_exchange";
    boardcast_exchange = "erizo_boardcast_exchange";

    erizo_controller_worker_num = 5;
    socket_io_thread_num = 20;
    erizo_agent_timeout = 10000;
    erizo_controller_update_interval = 1000;
    erizo_controller_timeout = 3000;
}

Config *Config::getInstance()
{
    if (instance_ == nullptr)
        instance_ = new Config;
    return instance_;
}

Config::~Config()
{
}

int Config::init(const std::string &config_file)
{
    std::ifstream ifs(config_file, std::ios::binary);
    if (!ifs.is_open())
    {
        ELOG_ERROR("open %s failed", config_file);
        return 1;
    }

    Json::Reader reader(Json::Features::strictMode());
    Json::Value root;
    if (!reader.parse(ifs, root))
    {
        ELOG_ERROR("parse %s failed", config_file);
        return 1;
    }

    Json::Value websocket = root["websocket"];
    if (!root.isMember("websocket") ||
        websocket.type() != Json::objectValue ||
        !websocket.isMember("port") ||
        websocket["port"].type() != Json::intValue ||
        !websocket.isMember("ssl") ||
        websocket["ssl"].type() != Json::booleanValue ||
        !websocket.isMember("ssl_key") ||
        websocket["ssl_key"].type() != Json::stringValue ||
        !websocket.isMember("ssl_cert") ||
        websocket["ssl_cert"].type() != Json::stringValue ||
        !websocket.isMember("ssl_passwd") ||
        websocket["ssl_passwd"].type() != Json::stringValue ||
        !websocket.isMember("ssl_port") ||
        websocket["ssl_port"].type() != Json::intValue)
    {
        ELOG_ERROR("websocket config check error");
        return 1;
    }

    Json::Value mysql = root["mysql"];
    if (!root.isMember("mysql") ||
        mysql.type() != Json::objectValue ||
        !mysql.isMember("url") ||
        mysql["url"].type() != Json::stringValue ||
        !mysql.isMember("username") ||
        mysql["username"].type() != Json::stringValue ||
        !mysql.isMember("password") ||
        mysql["password"].type() != Json::stringValue)
    {
        ELOG_ERROR("mysql config check error");
        return 1;
    }

    Json::Value redis = root["redis"];
    if (!root.isMember("redis") ||
        redis.type() != Json::objectValue ||
        !redis.isMember("ip") ||
        redis["ip"].type() != Json::stringValue ||
        !redis.isMember("port") ||
        redis["port"].type() != Json::intValue ||
        !redis.isMember("password") ||
        redis["password"].type() != Json::stringValue ||
        !redis.isMember("conn_timeout") ||
        redis["conn_timeout"].type() != Json::intValue ||
        !redis.isMember("rw_timeout") ||
        redis["rw_timeout"].type() != Json::intValue ||
        !redis.isMember("max_conns") ||
        redis["max_conns"].type() != Json::intValue ||
        !redis.isMember("lock_timeout") ||
        redis["lock_timeout"].type() != Json::intValue ||
        !redis.isMember("lock_try_time") ||
        redis["lock_try_time"].type() != Json::intValue)
    {
        ELOG_ERROR("redis config check error");
        return 1;
    }

    Json::Value rabbitmq = root["rabbitmq"];
    if (!root.isMember("rabbitmq") ||
        rabbitmq.type() != Json::objectValue ||
        !rabbitmq.isMember("host") ||
        rabbitmq["host"].type() != Json::stringValue ||
        !rabbitmq.isMember("port") ||
        rabbitmq["port"].type() != Json::intValue ||
        !rabbitmq.isMember("username") ||
        rabbitmq["username"].type() != Json::stringValue ||
        !rabbitmq.isMember("password") ||
        rabbitmq["password"].type() != Json::stringValue ||
        !rabbitmq.isMember("timeout") ||
        rabbitmq["timeout"].type() != Json::intValue ||
        !rabbitmq.isMember("boardcast_exchange") ||
        rabbitmq["boardcast_exchange"].type() != Json::stringValue ||
        !rabbitmq.isMember("uniquecast_exchange") ||
        rabbitmq["uniquecast_exchange"].type() != Json::stringValue)
    {
        ELOG_ERROR("rabbitmq config check error");
        return 1;
    }

    Json::Value other = root["other"];
    if (!root.isMember("other") ||
        other.type() != Json::objectValue ||
        !other.isMember("erizo_controller_worker_num") ||
        other["erizo_controller_worker_num"].type() != Json::intValue ||
        !other.isMember("socket_io_thread_num") ||
        other["socket_io_thread_num"].type() != Json::intValue ||
        !other.isMember("server") ||
        other["server"].type() != Json::arrayValue ||
        !other.isMember("erizo_agent_timeout") ||
        other["erizo_agent_timeout"].type() != Json::intValue ||
        !other.isMember("erizo_controller_update_interval") ||
        other["erizo_controller_update_interval"].type() != Json::intValue ||
        !other.isMember("erizo_controller_timeout") ||
        other["erizo_controller_timeout"].type() != Json::intValue)
    {
        ELOG_ERROR("other config check error");
        return 1;
    }

    port = websocket["port"].asInt();
    ssl = websocket["ssl"].asBool();
    ssl_key = websocket["ssl_key"].asString();
    ssl_cert = websocket["ssl_cert"].asString();
    ssl_passwd = websocket["ssl_passwd"].asString();
    ssl_port = websocket["ssl_port"].asInt();

    mysql_url = mysql["url"].asString();
    mysql_username = mysql["username"].asString();
    mysql_passwd = mysql["password"].asString();

    redis_ip = redis["ip"].asString();
    redis_port = redis["port"].asInt();
    redis_passwd = redis["password"].asString();
    redis_conn_timeout = redis["conn_timeout"].asInt();
    redis_rw_timeout = redis["rw_timeout"].asInt();
    redis_max_conns = redis["max_conns"].asInt();
    redis_lock_timeout = redis["lock_timeout"].asInt();
    redis_lock_try_time = redis["lock_try_time"].asInt();

    rabbitmq_hostname = rabbitmq["host"].asString();
    rabbitmq_port = rabbitmq["port"].asInt();
    rabbitmq_username = rabbitmq["username"].asString();
    rabbitmq_passwd = rabbitmq["password"].asString();
    rabbitmq_timeout = rabbitmq["timeout"].asInt();
    uniquecast_exchange = rabbitmq["uniquecast_exchange"].asString();
    boardcast_exchange = rabbitmq["boardcast_exchange"].asString();

    erizo_controller_worker_num = other["erizo_controller_worker_num"].asInt();
    socket_io_thread_num = other["socket_io_thread_num"].asInt();
    erizo_agent_timeout = other["erizo_agent_timeout"].asInt();
    erizo_controller_timeout = other["erizo_controller_timeout"].asInt();
    erizo_controller_update_interval = other["erizo_controller_update_interval"].asInt();

    Json::Value server_items = other["server"];
    for (size_t i = 0; i < server_items.size(); i++)
    {
        if (server_items[(int)i].type() != Json::objectValue)
            continue;

        Json::Value item = server_items[(int)i];
        if (!item.isMember("id") || item["id"].type() != Json::intValue ||
            !item.isMember("name") || item["name"].type() != Json::stringValue)
            continue;
        int id = item["id"].asInt();
        std::string name = item["name"].asString();
        server_mapping[id] = name;
    }

    return 0;
}
