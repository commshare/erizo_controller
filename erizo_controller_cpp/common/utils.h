#ifndef UTILS_H
#define UTILS_H

#include <iostream>
#include <string>
#include <sstream>
#include <chrono>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/regex.hpp>

#include <json/json.h>

#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "logger.h"

#define LOGGER_DECLARE() \
    static log4cxx::LoggerPtr logger;
#define LOGGER_INIT()                               \
    do                                              \
    {                                               \
        char buf[1024];                             \
        pid_t pid = getpid();                       \
        sprintf(buf, "[erizo-controller-%d]", pid); \
        logger = log4cxx::Logger::getLogger(buf);   \
    } while (0)

class Utils
{
  public:
    static std::string getUUID()
    {
        boost::uuids::uuid uuid = boost::uuids::random_generator()();
        return boost::uuids::to_string(uuid);
    }

    static bool searchAddress(const std::string &str, std::string &ip)
    {
        boost::regex reg("(25[0-4]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[1-9])[.](25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9])[.](25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[0-9])[.](25[0-4]|2[0-4][0-9]|1[0-9][0-9]|[1-9][0-9]|[1-9])");

        std::string::const_iterator start, end;
        start = str.begin();
        end = str.end();

        boost::match_results<std::string::const_iterator> what;
        boost::match_flag_type flags = boost::match_default;

        while (regex_search(start, end, what, reg, flags))
        {
            ip = std::string(what[0].first, what[0].second);
            return true;
        }
        return false;
    }

    static uint64_t getCurrentMs()
    {
        auto now = std::chrono::steady_clock::now();
        auto now_since_epoch = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now_since_epoch).count();
    }

    static uint64_t getSystemMs()
    {
        auto now = std::chrono::system_clock::now();
        auto now_since_epoch = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now_since_epoch).count();
    }

    static std::string getStreamID()
    {
        std::stringstream oss;
        for (int i = 0; i < 18; i++)
        {
            if (i == 0)
            {
                oss << (rand() % 9 + 1);
                continue;
            }
            oss << rand() % 10;
        }
        return oss.str();
    }

    static int initPath()
    {
        char buf[256] = {0};
        char filepath[256] = {0};
        char cmd[256] = {0};
        FILE *fp = NULL;

        sprintf(filepath, "/proc/%d", getpid());
        if (chdir(filepath) < 0)
            return 1;

        snprintf(cmd, 256, "ls -l | grep exe | awk '{print $11}'");
        if ((fp = popen(cmd, "r")) == nullptr)
            return 1;

        if (fgets(buf, sizeof(buf) / sizeof(buf[0]), fp) == nullptr)
        {
            pclose(fp);
            return 1;
        }

        std::string path = buf;
        size_t pos = path.find_last_of('/');
        if (pos != path.npos)
            path = path.substr(0, pos);

        if (chdir(path.c_str()) < 0)
            return 1;

        return 0;
    }

    static std::string dumpJson(const Json::Value &root)
    {
        Json::FastWriter writer;
        return writer.write(root);
    }
};

#endif