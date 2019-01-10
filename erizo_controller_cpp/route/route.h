#ifndef ROUTE_H
#define ROUTE_H

#include "common/logger.h"
#include "IpTable.h"

class Route
{
    DECLARE_LOGGER();

  public:
    static Route *getInstance();
    ~Route();

    int init();
    edu::iptable::IP_TABLE_VALUE processIP(const std::string &ip);

  private:
    Route();

  private:
    edu::iptable::IpTable ip_table_;
    static Route *instance_;
};

#endif