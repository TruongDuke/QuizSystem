#ifndef EXAM_MANAGER_H
#define EXAM_MANAGER_H

#include <string>

class DbManager;

// Exam operations
int startExam(int userId, int quizId, DbManager* db);
int getRemainingTime(int examId, DbManager* db);
void checkExpiredExams(DbManager* db);
void submitExam(int examId, DbManager* db);
void calculateScore(int examId, DbManager* db);
std::vector<int> getQuestionsForQuiz(int quizId, DbManager* db);

#endif // EXAM_MANAGER_H

