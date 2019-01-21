#include "config.h"

#include <fstream>

DEFINE_LOGGER(Config, "Config");

Config *Config::instance_ = nullptr;

Config::Config()
{
    port_ = 8080;
    ssl_key_ = "cert/key.pem";
    ssl_cert_ = "cert/cert.pem";
    ssl_port_ = 443;

    mysql_url_ = "tcp://119.28.70.47:3306";
    mysql_username_ = "root";
    mysql_password_ = "cathy978";

    redis_ip_ = "127.0.0.1";
    redis_port_ = 6379;
    redis_password_ = "cathy978";
    redis_conn_timeout_ = 10;
    redis_rw_timeout_ = 10;
    redis_max_conns_ = 100;

    rabbitmq_username_ = "linmin";
    rabbitmq_passwd_ = "linmin";
    rabbitmq_hostname_ = "localhost";
    rabbitmq_port_ = 5672;
    rabbitmq_timeout_ = 1000;
    uniquecast_exchange_ = "erizo_uniquecast_exchange";
    boardcast_exchange_ = "erizo_boardcast_exchange";

    worker_num_ = 5;
    thread_num_ = 20;
    default_area_ = "default_area";
    erizo_agent_timeout_ = 10000;
}

Config *Config::getInstance()
{
    if (instance_ == nullptr)
        instance_ = new Config;
    return instance_;
}

Config::~Config()
{
    if (instance_ != nullptr)
    {
        delete instance_;
        instance_ = nullptr;
    }
}

int Config::init(const std::string &config_file)
{
    std::ifstream ifs(config_file, std::ios::binary);
    if (!ifs.is_open())
    {
        ELOG_ERROR("Open %s failed", config_file);
        return 1;
    }

    Json::Reader reader;
    Json::Value root;
    if (!reader.parse(ifs, root))
    {
        ELOG_ERROR("Parse %s failed", config_file);
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
        ELOG_ERROR("Websocket config check error");
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
        ELOG_ERROR("Mysql config check error");
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
        redis["max_conns"].type() != Json::intValue)
    {
        ELOG_ERROR("Redis config check error");
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
        ELOG_ERROR("Rabbitmq config check error");
        return 1;
    }

    Json::Value other = root["other"];
    if (!root.isMember("other") ||
        other.type() != Json::objectValue ||
        !other.isMember("worker_num") ||
        other["worker_num"].type() != Json::intValue ||
        !other.isMember("thread_num") ||
        other["thread_num"].type() != Json::intValue ||
        !other.isMember("default_area") ||
        other["default_area"].type() != Json::stringValue ||
        !other.isMember("erizo_agent_timeout") ||
        other["erizo_agent_timeout"].type() != Json::intValue)
    {
        ELOG_ERROR("Other config check error");
        return 1;
    }

    port_ = websocket["port"].asInt();
    ssl_ = websocket["ssl"].asBool();
    ssl_key_ = websocket["ssl_key"].asString();
    ssl_cert_ = websocket["ssl_cert"].asString();
    ssl_passwd_ = websocket["ssl_passwd"].asString();
    ssl_port_ = websocket["ssl_port"].asInt();

    mysql_url_ = mysql["url"].asString();
    mysql_username_ = mysql["username"].asString();
    mysql_password_ = mysql["password"].asString();

    redis_ip_ = redis["ip"].asString();
    redis_port_ = redis["port"].asInt();
    redis_password_ = redis["password"].asString();
    redis_conn_timeout_ = redis["conn_timeout"].asInt();
    redis_rw_timeout_ = redis["rw_timeout"].asInt();
    redis_max_conns_ = redis["max_conns"].asInt();

    rabbitmq_hostname_ = rabbitmq["host"].asString();
    rabbitmq_port_ = rabbitmq["port"].asInt();
    rabbitmq_username_ = rabbitmq["username"].asString();
    rabbitmq_passwd_ = rabbitmq["password"].asString();
    rabbitmq_timeout_ = rabbitmq["timeout"].asInt();
    uniquecast_exchange_ = rabbitmq["uniquecast_exchange"].asString();
    boardcast_exchange_ = rabbitmq["boardcast_exchange"].asString();

    worker_num_ = other["worker_num"].asInt();
    thread_num_ = other["thread_num"].asInt();
    default_area_ = other["default_area"].asString();
    erizo_agent_timeout_ = other["erizo_agent_timeout"].asInt();

    return 0;
}
