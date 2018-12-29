#ifndef SQL_HELPER_H
#define SQL_HELPER_H

#include <string>
#include <thread>
#include <mutex>

#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/prepared_statement.h>

#include "common/logger.h"

class SQLHelper
{
  DECLARE_LOGGER();

public:
  static SQLHelper *getInstance();
  ~SQLHelper();

  int init();
  void close();

  bool isClientExist(const std::string &client_id);
  bool insertClient(const std::string &client_id, const std::string &room_id);

private:
  SQLHelper();

private:
  sql::Connection *conn_;
  std::mutex mux_;
  static SQLHelper *instance_;
};

#endif