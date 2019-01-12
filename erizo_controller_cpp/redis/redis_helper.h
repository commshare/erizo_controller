#ifndef REDIS_HELPER_H
#define REDIS_HELPER_H

#include <memory>
#include <thread>
#include <mutex>

#include <boost/asio/io_service.hpp>
#include <redisclient/redissyncclient.h>

#include "model/publisher.h"
#include "model/subscriber.h"
#include "model/room.h"
#include "model/client.h"
#include "model/erizo_agent.h"

#include "common/logger.h"

class RedisHelper
{
  DECLARE_LOGGER();

public:
  RedisHelper();
  ~RedisHelper();

  int init();
  void close();

  int addClient(const std::string &room_id, const Client &client);
  int removeClient(const std::string &room_id, const std::string &client_id);
  int getAllClient(const std::string &room_id, std::vector<Client> &clients);

  int addClientRoomMapping(const std::string &client_id, const std::string &room_id);
  int removeClientRoomMapping(const std::string &client_id);
  int getRoomByClientId(const std::string &client_id, std::string &room_id);

  int addPublisher(const std::string &room_id, const Publisher &publisher);
  int getPublisher(const std::string &room_id, const std::string &publisher_id, Publisher &publisher);
  int removePublishers(const std::string &room_id, const std::vector<std::string> &publishers);
  int getAllPublisher(const std::string &room_id, std::vector<Publisher> &publishers);

  int addSubscriber(const std::string &room_id, const Subscriber &subscriber);
  int removeSubscribers(const std::string &room_id, const std::vector<std::string> &subscribers);
  int getAllSubscriber(const std::string &room_id, std::vector<Subscriber> &subscribers);

  int getAllErizoAgent(const std::string &area, std::vector<ErizoAgent> &agents);

private:
  redisclient::RedisValue command(const std::string cmd, const std ::deque<redisclient::RedisBuffer> &buffer);

private:
  std::shared_ptr<redisclient::RedisSyncClient> redis_;
  bool init_;
  std::mutex mux_;
  boost::asio::io_service ios_;
};

#endif
