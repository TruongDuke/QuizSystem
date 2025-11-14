#include "db_manager.h"
#include <iostream>
 
DbManager::DbManager(const std::string& dbHost, const std::string& dbUser, const std::string& dbPass, const std::string& dbName)
    : dbHost(dbHost), dbUser(dbUser), dbPass(dbPass), dbName(dbName) {}
 
DbManager::~DbManager() {}
 
sql::Connection* DbManager::connectToDatabase() {
    try {
        sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
        sql::Connection *con = driver->connect("tcp://127.0.0.1:3306", dbUser, dbPass);
        con->setSchema(dbName);
        return con;
    } catch (sql::SQLException &e) {
        std::cerr << "Error connecting to MySQL: " << e.what() << std::endl;
        return nullptr;
    }
}
 
void DbManager::closeConnection(sql::Connection* con) {
    if (con) {
        delete con;
    }
}
 
sql::ResultSet* DbManager::executeQuery(const std::string& query) {
    sql::Connection* con = connectToDatabase();
    if (!con) {
        std::cerr << "Failed to connect to database!" << std::endl;
        return nullptr;
    }
 
    sql::PreparedStatement *stmt = con->prepareStatement(query);
    sql::ResultSet *res = stmt->executeQuery();
    return res;
}
 
void DbManager::executeUpdate(const std::string& query) {
    sql::Connection* con = connectToDatabase();
    if (!con) {
        std::cerr << "Failed to connect to database!" << std::endl;
        return;
    }
 
    sql::PreparedStatement *stmt = con->prepareStatement(query);
    stmt->executeUpdate();
}