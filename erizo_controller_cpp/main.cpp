#include "common/utils.h"
#include "common/logger.h"
#include "common/config.h"
#include "route/route.h"
#include "redis/acl_redis.h"
#include "core/erizo_controller.h"

static log4cxx::LoggerPtr logger;
static bool run = true;

void signal_handler(int signo)
{
  run = false;
}

int main()
{
  srand(time(0));
  signal(SIGINT, signal_handler);

  char buf[1024];
  pid_t pid = getpid();
  sprintf(buf, "[erizo-controller-%d]", pid);
  logger = log4cxx::Logger::getLogger(buf);

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
