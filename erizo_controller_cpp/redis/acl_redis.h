#ifndef ACL_REDIS_H
#define ACL_REDIS_H

#include "acl_cpp/lib_acl.hpp"

class ACLRedis
{
  public:
    int init();
    void close();

  private:
    acl::redis_client_cluster cluster_;
};

#endif