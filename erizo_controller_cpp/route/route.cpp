#include "route.h"

#include <arpa/inet.h>

DEFINE_LOGGER(Route, "Route");

int Route::init(const std::string &iptable_path)
{
    if (!ip_table_.loadIspIpDataFile(iptable_path))
    {
        ELOG_ERROR("load iptable failed");
        return 1;
    }

    return 0;
}

Route *Route::instance_ = nullptr;

Route *Route::getInstance()
{
    if (!instance_)
        instance_ = new Route;
    return instance_;
}

edu::iptable::IP_TABLE_VALUE Route::processIP(const std::string &ip)
{
    //only support ipv4
    struct in_addr in;
    inet_pton(AF_INET, ip.c_str(), &in);
    uint32_t ip_uint = in.s_addr;
    return ip_table_.getIpTableValue(ip_uint);
}

Route::Route() {}

Route::~Route() {}