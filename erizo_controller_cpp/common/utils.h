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

#include <stdlib.h>
#include <time.h>
#include <sys/syscall.h>

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

    static int gettid()
    {
        return syscall(__NR_gettid);
    }
};

#endif