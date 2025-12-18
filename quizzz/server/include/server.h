#ifndef SERVER_H
#define SERVER_H

#include <string>

class DbManager;
class SessionManager;

// Login handler
bool handleLogin(int clientSock, DbManager *db, std::string &outRole, std::string &outSessionId);

// Exam operations
void handleListExams(int sock, DbManager *db);

#endif // SERVER_H

