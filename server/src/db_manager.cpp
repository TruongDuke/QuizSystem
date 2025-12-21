#include "db_manager.h"
#include <iostream>
 
DbManager::DbManager(const std::string& dbHost,
                     const std::string& dbUser,
                     const std::string& dbPass,
                     const std::string& dbName)
    : dbHost(dbHost), dbUser(dbUser), dbPass(dbPass), dbName(dbName) {
    driver = sql::mysql::get_mysql_driver_instance();
    ensureConnection();
}

DbManager::~DbManager() {
    if (conn) {
        delete conn;
        conn = nullptr;
    }
}

void DbManager::ensureConnection() {
    if (!driver) {
        driver = sql::mysql::get_mysql_driver_instance();
    }

    if (!conn) {
        conn = driver->connect("tcp://" + dbHost + ":3306", dbUser, dbPass);
        conn->setSchema(dbName);
        return;
    }

    if (conn->isClosed()) {
        delete conn;
        conn = driver->connect("tcp://" + dbHost + ":3306", dbUser, dbPass);
        conn->setSchema(dbName);
    }
}
 
// ---------------------- SELECT ----------------------
// CHÚ Ý: để đơn giản mình không đóng connection ở đây,
// nên sẽ bị leak nhẹ, nhưng ổn cho bài demo.
sql::ResultSet* DbManager::executeQuery(const std::string& query) {
    std::lock_guard<std::mutex> lock(mtx);
    try {
        ensureConnection();
        sql::PreparedStatement* stmt = conn->prepareStatement(query);
        sql::ResultSet* res = stmt->executeQuery();

        // Không delete stmt ở đây vì ResultSet cần nó; caller sẽ delete res,
        // còn stmt sẽ được giải phóng khi connection đóng.
        return res;
    } catch (sql::SQLException& e) {
        std::cerr << "[DB] executeQuery error: " << e.what() << std::endl;
        return nullptr;
    }
}
 
// ------------------- INSERT/UPDATE/DELETE -------------------
void DbManager::executeUpdate(const std::string& query) {
    std::lock_guard<std::mutex> lock(mtx);
    try {
        ensureConnection();
        sql::PreparedStatement* stmt = conn->prepareStatement(query);
        stmt->executeUpdate();
        delete stmt;
    } catch (sql::SQLException& e) {
        std::cerr << "[DB] executeUpdate error: " << e.what() << std::endl;
    }
}
 
// ------------------- INSERT + LAST_INSERT_ID -------------------
int DbManager::executeInsertAndGetId(const std::string& query) {
    std::lock_guard<std::mutex> lock(mtx);
    try {
        ensureConnection();

        sql::PreparedStatement* stmt = conn->prepareStatement(query);
        stmt->executeUpdate();
        delete stmt;

        sql::PreparedStatement* stmt2 =
            conn->prepareStatement("SELECT LAST_INSERT_ID() AS id;");
        sql::ResultSet* res = stmt2->executeQuery();

        int id = 0;
        if (res && res->next()) {
            id = res->getInt("id");
        }

        delete res;
        delete stmt2;

        return id;
    } catch (sql::SQLException& e) {
        std::cerr << "[DB] executeInsertAndGetId error: "
<< e.what() << std::endl;
        return 0;
    }
}