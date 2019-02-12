#include "common/utils.h"
#include "common/logger.h"
#include "common/config.h"
#include "route/route.h"
#include "redis/acl_redis.h"
#include "core/erizo_controller.h"

LOGGER_DECLARE()

static bool run = true;

void signal_handler(int signo)
{
  run = false;
}

int main()
{
  srand(time(0));
  signal(SIGINT, signal_handler);

  LOGGER_INIT();

  if (Utils::initPath())
  {
    ELOG_ERROR("working path initialize failed");
    return 1;
  }

  if (Config::getInstance()->init("config.json"))
  {
    ELOG_ERROR("load configure file failed");
    return 1;
  }

  if (Route::getInstance()->init("iptable"))
  {
    ELOG_ERROR("load iptable file failed");
    return 1;
  }

  if (ACLRedis::getInstance()->init())
  {
    ELOG_ERROR("acl-redis initialize failed");
    return 1;
  }

  if (ErizoController::getInstance()->init())
  {
    ELOG_ERROR("erizo-controller initialize failed");
    return 1;
  }

  while (run)
    sleep(10000);

  ErizoController::getInstance()->close();
  ACLRedis::getInstance()->close();
  return 0;
}
