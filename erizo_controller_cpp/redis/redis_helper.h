#ifndef REDIS_HELPER_H
#define REDIS_HELPER_H

#include "core/erizo_controller.h"
#include "model/publisher.h"
#include "model/subscriber.h"
#include "model/room.h"
#include "model/client.h"
#include "model/erizo_agent.h"
#include "model/bridge_stream.h"

class RedisHelper
{
public:
  static int addClient(const std::string &room_id, const Client &client);
  static int removeClient(const std::string &room_id, const std::string &client_id);
  static int getAllClient(const std::string &room_id, std::vector<Client> &clients);

  static int addPublisher(const std::string &room_id, const Publisher &publisher);
  static int getPublisher(const std::string &room_id, const std::string &publisher_id, Publisher &publisher);
  static int removePublishers(const std::string &room_id, const std::vector<std::string> &publisher_ids);
  static int getAllPublisher(const std::string &room_id, std::vector<Publisher> &publishers);

  static int addSubscriber(const std::string &room_id, const Subscriber &subscriber);
  static int removeSubscribers(const std::string &room_id, const std::vector<std::string> &subscriber_ids);
  static int getAllSubscriber(const std::string &room_id, std::vector<Subscriber> &subscribers);

  static int getAllErizoAgent(const std::string &area, std::vector<ErizoAgent> &agents);
  // static int removeErizoAgent(const std::string &area, const ErizoAgent &agent);
  // static int removeAllErizo(const ErizoAgent &agent);

  static int addBridgeStream(const std::string &room_id, const BridgeStream &bridge_stream);
  static int getBridgeStream(const std::string &room_id, const std::string &bridge_stream_id, BridgeStream &bridge_stream);
  static int removeBridgeStream(const std::string &room_id, const std::string &bridge_stream_id);
  static int getAllBridgeStream(const std::string &room_id, std::vector<BridgeStream> &bridge_streams);

  static int addClientToEC(const std::string &erizo_controller_id, const Client &client);
  static int removeClientFromEC(const std::string &erizo_controller_id, const std::string &client_id);
  static int getAllClientFromEC(const std::string &erizo_controller_id, std::vector<Client> &clients);

  static int addHeartbeatData(const ErizoController::HEARTBEAT &heartbeat_data);
  static int removeHeartbeatData(const std::string &erizo_controller_id);
  static int getAllHeartbeatData(std::vector<ErizoController::HEARTBEAT> &heartbeats);
};

#endif
