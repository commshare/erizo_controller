#include "acl_redis.h"
#include "common/config.h"

int ACLRedis::init()
{
    acl::acl_cpp_init();

    char addr_buf[2048];
    sprintf(addr_buf, "%s:%d", Config::getInstance()->redis_ip_.c_str(), Config::getInstance()->redis_port_);

    cluster_.set(addr_buf,
                 Config::getInstance()->redis_conn_timeout_,
                 Config::getInstance()->redis_rw_timeout_,
                 Config::getInstance()->redis_max_conns_);
    return 0;
}