#include "core/erizo_controller.h"
#include "common/config.h"
#include "common/utils.h"
#include "route/route.h"
int main()
{
  Utils::initPath();
  Config::getInstance()->init("config.json");
  Route::getInstance()->init("iptable");
  srand(time(0));
  ErizoController ec;
  ec.init();
  sleep(100000);
  ec.close();
  return 0;
}
