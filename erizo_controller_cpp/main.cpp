#include "core/erizo_controller.h"

int main()
{
  Config::getInstance()->init("config.json");
  srand(time(0));
  ErizoController ec;
  ec.init();
  sleep(100000);
  ec.close();
  return 0;
}
