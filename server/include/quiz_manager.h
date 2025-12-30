#ifndef QUIZ_MANAGER_H
#define QUIZ_MANAGER_H

#include <string>
#include <vector>

class DbManager;

// Quiz operations
void handleListQuizzes(int sock, DbManager *db);
void handleAddQuiz(const std::vector<std::string> &parts, int sock, DbManager *db);
void handleEditQuiz(const std::vector<std::string> &parts, int sock, DbManager *db);
void handleDeleteQuiz(const std::vector<std::string> &parts, int sock, DbManager *db);
void handleStatusQuiz(const std::vector<std::string> &parts, int sock, DbManager *db);
void updateScheduledQuizStatus(DbManager *db);

#endif // QUIZ_MANAGER_H

