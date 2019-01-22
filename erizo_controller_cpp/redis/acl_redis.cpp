#include "acl_redis.h"
#include "common/config.h"

#include <sstream>

ACLRedis::ACLRedis() : cluster_(nullptr),
                       init_(false)
{
}

int ACLRedis::init()
{
    if (init_)
        return 0;
    acl::acl_cpp_init();
    std::ostringstream oss;
    oss << Config::getInstance()->redis_ip_ << ":" << Config::getInstance()->redis_port_;

    cluster_ = std::make_shared<acl::redis_client_cluster>();
    cluster_->set(oss.str().c_str(),
                  Config::getInstance()->redis_max_conns_,
                  Config::getInstance()->redis_conn_timeout_,
                  Config::getInstance()->redis_rw_timeout_);
    cluster_->set_password("default", Config::getInstance()->redis_password_.c_str());
    init_ = true;
    return 0;
}

void ACLRedis::close()
{
    if (!init_)
        return;
    cluster_.reset();
    cluster_ = nullptr;
    init_ = false;
}

ACLRedis::~ACLRedis()
{
}

ACLRedis *ACLRedis::instance_ = nullptr;
ACLRedis *ACLRedis::getInstance()
{
    if (!instance_)
        instance_ = new ACLRedis;
    return instance_;
}

int ACLRedis::setnx(const std::string &key, const std::string &value)
{
    if (!init_)
        return false;

    acl::redis_string cmd;
    cmd.set_cluster(cluster_.get(), Config::getInstance()->redis_max_conns_);
    int res = cmd.setnx(key.c_str(), value.c_str());
    cmd.clear();
    return res;
}

int ACLRedis::set(const std::string &key, const std::string &value)
{
    if (!init_)
        return false;

    acl::redis_string cmd;
    cmd.set_cluster(cluster_.get(), Config::getInstance()->redis_max_conns_);
    int res = cmd.set(key.c_str(), value.c_str());
    cmd.clear();
    return res;
}

int ACLRedis::get(const std::string &key, std::string &value)
{
    if (!init_)
        return false;

    acl::redis_string cmd;
    cmd.set_cluster(cluster_.get(), Config::getInstance()->redis_max_conns_);
    acl::string buf;
    int res = cmd.get(key.c_str(), buf);
    cmd.clear();

    if (!buf.empty())
        value = buf.c_str();

    return res;
}

int ACLRedis::getset(const std::string &key, const std::string &value, std::string &old_value)
{
    if (!init_)
        return false;
    acl::redis_string cmd;
    cmd.set_cluster(cluster_.get(), Config::getInstance()->redis_max_conns_);
    acl::string buf;
    int res = cmd.getset(key.c_str(), value.c_str(), buf);
    cmd.clear();

    if (!buf.empty())
        old_value = buf.c_str();
    return res;
}

int ACLRedis::del(const std::string &key)
{
    if (!init_)
        return false;
    acl::redis cmd;
    cmd.set_cluster(cluster_.get(), Config::getInstance()->redis_max_conns_);
    int res = cmd.del_one(key.c_str());
    cmd.clear();
    return res;
}

int ACLRedis::hset(const std::string &key, const std::string &field, const std::string &value)
{
    if (!init_)
        return false;
    acl::redis_hash cmd;
    cmd.set_cluster(cluster_.get(), Config::getInstance()->redis_max_conns_);
    int res = cmd.hset(key.c_str(), field.c_str(), value.c_str());
    cmd.clear();
    return res;
}

int ACLRedis::hget(const std::string &key, const std::string &field, std::string &value)
{
    if (!init_)
        return false;
    acl::redis_hash cmd;
    cmd.set_cluster(cluster_.get(), Config::getInstance()->redis_max_conns_);
    acl::string buf;
    int res = cmd.hget(key.c_str(), field.c_str(), buf);
    cmd.clear();

    if (!buf.empty())
        value = buf.c_str();
    return res;
}

int ACLRedis::hdel(const std::string &key, const std::string &field)
{
    if (!init_)
        return false;
    acl::redis_hash cmd;
    cmd.set_cluster(cluster_.get(), Config::getInstance()->redis_max_conns_);
    int res = cmd.hdel(key.c_str(), field.c_str());
    cmd.clear();
    return res;
}

int ACLRedis::hdel(const std::string &key, const std::vector<std::string> &fields)
{
    if (!init_)
        return false;
    acl::redis_hash cmd;
    cmd.set_cluster(cluster_.get(), Config::getInstance()->redis_max_conns_);
    std::vector<acl::string> acl_fields;
    for (const std::string &s : fields)
        acl_fields.push_back(s.c_str());
    int res = cmd.hdel_fields(key.c_str(), acl_fields);
    cmd.clear();
    return res;
}

int ACLRedis::hvals(const std::string &key, std::vector<std::string> &fields, std::vector<std::string> &values)
{
    if (!init_)
        return false;
    acl::redis_hash cmd;
    cmd.set_cluster(cluster_.get(), Config::getInstance()->redis_max_conns_);
    std::map<acl::string, acl::string> buf;
    int res = cmd.hgetall(key.c_str(), buf);
    cmd.clear();

    for (auto it = buf.begin(); it != buf.end(); it++)
    {
        fields.push_back(std::string(it->first.c_str()));
        values.push_back(std::string(it->second.c_str()));
    }
    return res;
}