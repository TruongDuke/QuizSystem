#ifndef QUESTION_MANAGER_H
#define QUESTION_MANAGER_H

#include <string>
#include <vector>

class DbManager;

// Question operations
void handleListQuestions(const std::vector<std::string> &parts, int sock, DbManager *db);
void handleAddQuestion(const std::vector<std::string> &parts, int sock, DbManager *db);
void handleEditQuestion(const std::vector<std::string> &parts, int sock, DbManager *db);
void handleDeleteQuestion(const std::vector<std::string> &parts, int sock, DbManager *db);

#endif // QUESTION_MANAGER_H

