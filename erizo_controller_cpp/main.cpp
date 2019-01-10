#include "core/erizo_controller.h"
#include "common/config.h"
#include "common/utils.h"

int main()
{
  Utils::initPath();
  Config::getInstance()->init("config.json");
  srand(time(0));
  ErizoController ec;
  ec.init();
  sleep(100000);
  ec.close();
  return 0;
}
