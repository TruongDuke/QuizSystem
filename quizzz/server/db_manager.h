#ifndef DB_MANAGER_H
#define DB_MANAGER_H
 
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <string>
 
class DbManager {
public:
    DbManager(const std::string& dbHost, const std::string& dbUser, const std::string& dbPass, const std::string& dbName);
    ~DbManager();
 
    sql::Connection* connectToDatabase();
    void closeConnection(sql::Connection* con);
 
    sql::ResultSet* executeQuery(const std::string& query);
    void executeUpdate(const std::string& query);
 
private:
    std::string dbHost;
    std::string dbUser;
    std::string dbPass;
    std::string dbName;
};
 
#endif // DB_MANAGER_H