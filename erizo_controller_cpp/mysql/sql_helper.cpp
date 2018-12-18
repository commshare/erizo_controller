#include "sql_helper.h"

#include "common/config.h"

DEFINE_LOGGER(SQLHelper, "SQLHelper");
SQLHelper *SQLHelper::instance_ = nullptr;
SQLHelper *SQLHelper::getInstance()
{
    if (instance_ == nullptr)
        instance_ = new SQLHelper;
    return instance_;
}

SQLHelper::~SQLHelper() {}

SQLHelper::SQLHelper() : conn_(nullptr)
{
}

int SQLHelper::init()
{

    try
    {
        sql::Driver *driver = get_driver_instance();
        conn_ = driver->connect(Config::getInstance()->mysql_url_,
                                Config::getInstance()->mysql_username_,
                                Config::getInstance()->mysql_password_);
        conn_->setSchema("licode");
        return 0;
    }
    catch (sql::SQLException &e)
    {
        std::cout << "SQL ERROR CODE:" << e.getErrorCode() << ",SQLState:" << e.getSQLState() << std::endl;
        return 1;
    }
}

void SQLHelper::close()
{
    delete conn_;
    conn_ = nullptr;
}

bool SQLHelper::isClientExist(const std::string &client_id)
{
    try
    {
        std::shared_ptr<sql::ResultSet> res;
        {
            std::shared_ptr<sql::PreparedStatement> pst;
            std::unique_lock<std::mutex> lock(mux_);
            pst.reset(conn_->prepareStatement("select 1 from client where id = ? limit 1"));
            pst->setString(1, client_id);
            res.reset(pst->executeQuery());
        }
        while (res->next())
            return true;
        return false;
    }
    catch (sql::SQLException &e)
    {
        std::cout << "SQL ERROR:" << e.what() << std::endl;
        return false;
    }
}

bool SQLHelper::insertClient(const std::string &client_id, const std::string &room_id)
{
    try
    {
        std::shared_ptr<sql::PreparedStatement> pst;

        std::unique_lock<std::mutex> lock(mux_);
        pst.reset(conn_->prepareStatement("insert into client(id,room_id) values(?,?)"));
        pst->setString(1, client_id);
        pst->setString(2, room_id);
        pst->executeUpdate();

        return true;
    }
    catch (sql::SQLException &e)
    {
        std::cout << "SQL ERROR:" << e.what() << std::endl;
        return false;
    }
}