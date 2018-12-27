#include "core/erizo_controller.h"

int main()
{
  Config::getInstance()->init("config.json");

  ErizoController ec;
  ec.init();
  sleep(100000);
  ec.close();
  return 0;
}


