#include "erizo_controller/erizo_controller.h"
#include "mysql/sql_helper.h"
#include "rabbitmq/rabbitmq_rpc.h"
int main()
{
  Config::getInstance()->init("config.json");

  ErizoController ec;
  ec.init();

  AMQPRPC rpc;
  rpc.init("rpcExchange", "direct");

  // bool b =helper->isClientExist("6666");
  // std::cout<<"ds:"<<b<<std::endl;
  Json::Value root;
  Json::Reader reader;
  if (!reader.parse("{\"method\":\"keepAlive\"}", root))
    return 1;

  rpc.addRPC("rpcExchange",
             "ErizoJS_666666666666",
             "ErizoJS_666666666666",
             root,
             [](const Json::Value &tt) {
               Json::FastWriter writer;
               std::string msg = writer.write(tt);

               printf("%s\n", msg.c_str());
             });

  sleep(100000);

  printf("stop websocket\n");
  ec.close();
  return 0;
}
