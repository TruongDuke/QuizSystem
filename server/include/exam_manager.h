#ifndef EXAM_MANAGER_H
#define EXAM_MANAGER_H

#include <string>
#include <vector>

class DbManager;
class ClientManager;

// Exam operations
int startExam(int userId, int quizId, DbManager* db);
int getRemainingTime(int examId, DbManager* db);
void checkExpiredExams(DbManager* db, ClientManager& clientMgr);
void submitExam(int examId, DbManager* db);
void calculateScore(int examId, DbManager* db);
std::vector<int> getQuestionsForQuiz(int quizId, DbManager* db);
void handleGetQuizStats(const std::vector<std::string>& parts, int sock, DbManager* db);

#endif // EXAM_MANAGER_H

