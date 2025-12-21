#include "db_manager.h"
#include <iostream>
 
DbManager::DbManager(const std::string& dbHost,
                     const std::string& dbUser,
                     const std::string& dbPass,
                     const std::string& dbName)
    : dbHost(dbHost), dbUser(dbUser), dbPass(dbPass), dbName(dbName) {}
 
DbManager::~DbManager() {}
 
// ---------------------- SELECT ----------------------
// CHÚ Ý: để đơn giản mình không đóng connection ở đây,
// nên sẽ bị leak nhẹ, nhưng ổn cho bài demo.
sql::ResultSet* DbManager::executeQuery(const std::string& query) {
    try {
        sql::mysql::MySQL_Driver* driver =
            sql::mysql::get_mysql_driver_instance();
 
        sql::Connection* con =
            driver->connect("tcp://" + dbHost + ":3306", dbUser, dbPass);
        con->setSchema(dbName);
 
        sql::PreparedStatement* stmt = con->prepareStatement(query);
        sql::ResultSet* res = stmt->executeQuery();
 
        // KHÔNG delete con / stmt ở đây vì res vẫn cần chúng.
        // Demo nhỏ nên chấp nhận leak.
        return res;
    } catch (sql::SQLException& e) {
        std::cerr << "[DB] executeQuery error: " << e.what() << std::endl;
        return nullptr;
    }
}
 
// ------------------- INSERT/UPDATE/DELETE -------------------
void DbManager::executeUpdate(const std::string& query) {
    try {
        sql::mysql::MySQL_Driver* driver =
            sql::mysql::get_mysql_driver_instance();
 
        sql::Connection* con =
            driver->connect("tcp://" + dbHost + ":3306", dbUser, dbPass);
        con->setSchema(dbName);
 
        sql::PreparedStatement* stmt = con->prepareStatement(query);
        stmt->executeUpdate();
 
        delete stmt;
        delete con;
    } catch (sql::SQLException& e) {
        std::cerr << "[DB] executeUpdate error: " << e.what() << std::endl;
        throw; // Re-throw exception để caller xử lý
    }
}
 
// ------------------- INSERT + LAST_INSERT_ID -------------------
int DbManager::executeInsertAndGetId(const std::string& query) {
    try {
        sql::mysql::MySQL_Driver* driver =
            sql::mysql::get_mysql_driver_instance();
 
        sql::Connection* con =
            driver->connect("tcp://" + dbHost + ":3306", dbUser, dbPass);
        con->setSchema(dbName);
 
        // Thực thi INSERT
        sql::PreparedStatement* stmt = con->prepareStatement(query);
        stmt->executeUpdate();
        delete stmt;
 
        // Lấy LAST_INSERT_ID() trên cùng connection
        sql::PreparedStatement* stmt2 =
            con->prepareStatement("SELECT LAST_INSERT_ID() AS id;");
        sql::ResultSet* res = stmt2->executeQuery();
 
        int id = 0;
        if (res && res->next()) {
            id = res->getInt("id");
        }
 
        delete res;
        delete stmt2;
        delete con;
 
        return id;
    } catch (sql::SQLException& e) {
        std::cerr << "[DB] executeInsertAndGetId error: "
<< e.what() << std::endl;
        return 0;
    }
}