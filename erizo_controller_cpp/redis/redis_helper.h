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

#include "common/logger.h"

class RedisHelper
{
  DECLARE_LOGGER();

public:
  RedisHelper();
  ~RedisHelper();

  int init();
  void close();

  int addRoom(const Room &room);
  int getRoom(const std::string &room_id, Room &room);
  int getAllRoom(std::vector<Room> &rooms);

  int addClient(const std::string &room_id, const std::string &client_id);
  int getAllClient(const std::string &room_id, std::vector<std::string> &client_ids);

  int addPublisher(const std::string &room_id, const Publisher &publisher);
  int getPublisher(const std::string &room_id, const std::string &publisher_id, Publisher &publisher);
  int getAllPublisher(const std::string &room_id, std::vector<Publisher> &publishers);

  int addSubscriber(const std::string &publisher_id, const Subscriber &subscriber);
  int removePublisher(const std::string &room_id, const std::string &publisher_id);
  int removeSubscriber(const std::string &publisher_id, const std::string &subscriber_id);
  int removeRoom(const std::string &room_id);

private:
  std::shared_ptr<redisclient::RedisSyncClient> redis_;
  bool init_;
  std::mutex mux_;
  boost::asio::io_service ios_;
};

#endif
