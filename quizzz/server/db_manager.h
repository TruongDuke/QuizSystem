#ifndef DB_MANAGER_H
#define DB_MANAGER_H
 
#include <string>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
 
class DbManager {
public:
    DbManager(const std::string& dbHost,
              const std::string& dbUser,
              const std::string& dbPass,
              const std::string& dbName);
    ~DbManager();
 
    // SELECT -> trả về ResultSet*
    sql::ResultSet* executeQuery(const std::string& query);
 
    // INSERT / UPDATE / DELETE không cần lấy id
    void executeUpdate(const std::string& query);
 
    // INSERT và lấy lại LAST_INSERT_ID()
    int executeInsertAndGetId(const std::string& query);
 
    // Alias cho tương thích code cũ
    int executeInsert(const std::string& query) {
        return executeInsertAndGetId(query);
    }
 
private:
    std::string dbHost;
    std::string dbUser;
    std::string dbPass;
    std::string dbName;
};

#endif // DB_MANAGER_H