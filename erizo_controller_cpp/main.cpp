#include "erizo_controller/erizo_controller.h"
#include "mysql/sql_helper.h"

int main()
{
  Config::getInstance()->init("config.json");

  ErizoController ec;
  ec.init();

  // AMQPRPC rpc;
  // rpc.init("rpcExchange", "direct");

  Json::Value root;
  Json::Reader reader;
  if (!reader.parse("{\"method\":\"keepAlive\"}", root))
    return 1;

  // rpc.addRPC("rpcExchange",
  //            "ErizoJS_666666666666",
  //            "ErizoJS_666666666666",
  //            root,
  //            [](const Json::Value &tt) {
  //            });

  sleep(100000);

  printf("stop websocket\n");
  ec.close();
  return 0;
}
