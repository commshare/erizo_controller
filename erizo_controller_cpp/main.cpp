#include "core/erizo_controller.h"
#include "common/config.h"
#include "common/utils.h"
#include "route/route.h"

#include "redis/acl_redis.h"
#include "redis/redis_locker.h"
int main()
{
  Utils::initPath();
  Config::getInstance()->init("config.json");
  Route::getInstance()->init("iptable");
  ACLRedis::getInstance()->init();
  srand(time(0));

  ErizoController ec;
  ec.init();
  sleep(100000);
  ec.close();

  return 0;
}
