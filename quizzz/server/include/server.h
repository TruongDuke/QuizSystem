#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <vector>

class DbManager;
class SessionManager;
class ClientManager;

// Login handler
bool handleLogin(int clientSock, DbManager *db, std::string &outRole, std::string &outSessionId);

// Exam operations
void handleListExams(int sock, DbManager *db);

// Command handler
void handleCommand(int clientSock, const std::vector<std::string>& parts, 
                  DbManager* db, ClientManager& clientMgr);

#endif // SERVER_H

