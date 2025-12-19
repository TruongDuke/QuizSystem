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

// Student exam handlers
void handleJoinRoom(const std::vector<std::string>& parts, int sock, 
                   DbManager* db, ClientManager& clientMgr);
void handleAnswer(const std::vector<std::string>& parts, int sock,
                 DbManager* db, ClientManager& clientMgr);
void handleListMyHistory(int sock, DbManager* db, ClientManager& clientMgr);

// Command handler
void handleCommand(int clientSock, const std::vector<std::string>& parts, 
                  DbManager* db, ClientManager& clientMgr);

#endif // SERVER_H

