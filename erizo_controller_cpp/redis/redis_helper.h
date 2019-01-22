#ifndef REDIS_HELPER_H
#define REDIS_HELPER_H

#include "model/publisher.h"
#include "model/subscriber.h"
#include "model/room.h"
#include "model/client.h"
#include "model/erizo_agent.h"
#include "model/bridge_stream.h"
#include "model/stream.h"

#include "acl_redis.h"
#include "redis_locker.h"

class RedisHelper
{
public:
  static int addClient(const std::string &room_id, const Client &client);
  static int removeClient(const std::string &room_id, const std::string &client_id);
  static int getAllClient(const std::string &room_id, std::vector<Client> &clients);

  static int addClientRoomMapping(const std::string &client_id, const std::string &room_id);
  // static bool isClientExist(const std::string &client_id);
  static int removeClientRoomMapping(const std::string &client_id);
  static int getRoomByClientId(const std::string &client_id, std::string &room_id);

  static int addPublisher(const std::string &room_id, const Publisher &publisher);
  static int getPublisher(const std::string &room_id, const std::string &publisher_id, Publisher &publisher);
  static int removePublishers(const std::string &room_id, const std::vector<std::string> &publisher_ids);
  static int getAllPublisher(const std::string &room_id, std::vector<Publisher> &publishers);

  static int addSubscriber(const std::string &room_id, const Subscriber &subscriber);
  static int removeSubscribers(const std::string &room_id, const std::vector<std::string> &subscriber_ids);
  static int getAllSubscriber(const std::string &room_id, std::vector<Subscriber> &subscribers);

  static int getAllErizoAgent(const std::string &area, std::vector<ErizoAgent> &agents);

  static int addBridgeStream(const std::string &room_id, const BridgeStream &bridge_stream);
  static int getBridgeStream(const std::string &room_id, const std::string &bridge_stream_id, BridgeStream &bridge_stream);
  static int getAllBridgeStream(const std::string &room_id, std::vector<BridgeStream> &bridge_streams);

  static int addStream(const std::string &client_id, const Stream &stream);
  static int getStream(const std::string &stream_id, Stream &stream);
};

#endif
