#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <vector>

class DbManager;

bool handleLogin(int clientSock, DbManager *db, std::string &outRole, int &outUserId);
void handleListExams(int sock, DbManager *db);
void handleStudentJoinExam(const std::vector<std::string> &parts, int sock, int userId, DbManager *db);
void handleStudentAnswer(const std::vector<std::string> &parts, int sock, DbManager *db);
void handleListHistory(int sock, int userId, DbManager *db);
void sendNextQuestion(int sock, DbManager *db);

#endif // SERVER_H

