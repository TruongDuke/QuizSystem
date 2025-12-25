#include "../include/exam_manager.h"
#include "../include/db_manager.h"
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/resultset.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cctype>

// ==================================================
// EXAM OPERATIONS
// ==================================================

// Start exam: tạo exam record với start_time = NOW()
int startExam(int userId, int quizId, DbManager* db) {
    try {
        // Check if user already has an in_progress exam for this quiz
        std::string checkSql = 
            "SELECT exam_id FROM Exams "
            "WHERE user_id = " + std::to_string(userId) + 
            " AND quiz_id = " + std::to_string(quizId) +
            " AND status = 'in_progress';";
        
        sql::ResultSet* checkRes = db->executeQuery(checkSql);
        if (checkRes && checkRes->next()) {
            int existingExamId = checkRes->getInt("exam_id");
            delete checkRes;
            std::cout << "[START_EXAM] User already has exam in progress: " << existingExamId << std::endl;
            return existingExamId;
        }
        delete checkRes;
        
        // Create new exam
        std::string sql = 
            "INSERT INTO Exams (quiz_id, user_id, start_time, status) "
            "VALUES (" + std::to_string(quizId) + ", " + 
            std::to_string(userId) + ", NOW(), 'in_progress');";
        
        int examId = db->executeInsertAndGetId(sql);
        std::cout << "[START_EXAM] Exam started: examId=" << examId 
                  << ", userId=" << userId << ", quizId=" << quizId << std::endl;
        return examId;
    } catch (sql::SQLException& e) {
        std::cerr << "[START_EXAM] SQL error: " << e.what() << std::endl;
        return 0;
    }
}

// Get remaining time in seconds
// For scheduled exams: takes minimum of (time_limit remaining) and (time until exam_end_time)
int getRemainingTime(int examId, DbManager* db) {
    try {
        std::string sql = 
            "SELECT "
            "  q.time_limit - TIMESTAMPDIFF(SECOND, e.start_time, NOW()) as time_limit_remaining, "
            "  q.exam_type, "
            "  CASE "
            "    WHEN q.exam_type = 'scheduled' AND q.exam_end_time IS NOT NULL "
            "    THEN TIMESTAMPDIFF(SECOND, NOW(), q.exam_end_time) "
            "    ELSE NULL "
            "  END as exam_end_remaining "
            "FROM Exams e "
            "JOIN Quizzes q ON e.quiz_id = q.quiz_id "
            "WHERE e.exam_id = " + std::to_string(examId) + 
            " AND e.status = 'in_progress';";
        
        sql::ResultSet* res = db->executeQuery(sql);
        if (res && res->next()) {
            int timeLimitRemaining = res->getInt("time_limit_remaining");
            std::string examType = res->getString("exam_type");
            
            int remaining = timeLimitRemaining;
            
            // For scheduled exams, take the minimum of time_limit and exam_end_time
            if (examType == "scheduled" && !res->isNull("exam_end_remaining")) {
                int examEndRemaining = res->getInt("exam_end_remaining");
                remaining = std::min(timeLimitRemaining, examEndRemaining);
            }
            
            delete res;
            return remaining > 0 ? remaining : 0; // Return 0 if time is up
        }
        delete res;
        return -1; // Exam does not exist or already submitted
    } catch (sql::SQLException& e) {
        std::cerr << "[GET_REMAINING_TIME] SQL error: " << e.what() << std::endl;
        return -1;
    }
}

// Check and auto-submit expired exams
void checkExpiredExams(DbManager* db) {
    try {
        // 1. Tìm các exam đã hết thời gian làm bài (time_limit)
        std::string findExpiredSql = 
            "SELECT e.exam_id, e.user_id, e.quiz_id "
            "FROM Exams e "
            "JOIN Quizzes q ON e.quiz_id = q.quiz_id "
            "WHERE e.status = 'in_progress' "
            "  AND NOW() >= DATE_ADD(e.start_time, INTERVAL q.time_limit SECOND);";
        
        sql::ResultSet* res = db->executeQuery(findExpiredSql);
        if (res) {
            std::vector<int> expiredExamIds;
            while (res->next()) {
                expiredExamIds.push_back(res->getInt("exam_id"));
            }
            delete res;
            
            // Auto-submit các exam đã hết thời gian làm bài
            for (int examId : expiredExamIds) {
                submitExam(examId, db);
                std::cout << "[TIMER] Auto-submitted expired exam (time_limit): " << examId << std::endl;
            }
        }
        
        // 2. Tìm các scheduled exam đã vượt quá exam_end_time
        std::string findScheduledExpiredSql = 
            "SELECT e.exam_id, e.user_id, e.quiz_id "
            "FROM Exams e "
            "JOIN Quizzes q ON e.quiz_id = q.quiz_id "
            "WHERE e.status = 'in_progress' "
            "  AND q.exam_type = 'scheduled' "
            "  AND q.exam_end_time IS NOT NULL "
            "  AND NOW() > q.exam_end_time;";
        
        sql::ResultSet* scheduledRes = db->executeQuery(findScheduledExpiredSql);
        if (scheduledRes) {
            std::vector<int> scheduledExpiredIds;
            while (scheduledRes->next()) {
                scheduledExpiredIds.push_back(scheduledRes->getInt("exam_id"));
            }
            delete scheduledRes;
            
            // Auto-submit các scheduled exam đã vượt quá exam_end_time
            for (int examId : scheduledExpiredIds) {
                submitExam(examId, db);
                std::cout << "[TIMER] Auto-submitted scheduled exam (past exam_end_time): " << examId << std::endl;
            }
        }
        
    } catch (sql::SQLException& e) {
        std::cerr << "[CHECK_EXPIRED_EXAMS] SQL error: " << e.what() << std::endl;
    }
}

// Submit exam manually or automatically
void submitExam(int examId, DbManager* db) {
    try {
        // Kiểm tra exam status trước - chỉ submit nếu chưa submitted
        std::string checkSql = 
            "SELECT status FROM Exams WHERE exam_id = " + 
            std::to_string(examId) + ";";
        sql::ResultSet* checkRes = db->executeQuery(checkSql);
        if (checkRes && checkRes->next()) {
            std::string status = checkRes->getString("status");
            if (status == "submitted") {
                std::cout << "[SUBMIT_EXAM] Exam " << examId 
                          << " already submitted, skipping" << std::endl;
                delete checkRes;
                return; // Đã submitted rồi, không làm gì
            }
        }
        delete checkRes;
        
        // Calculate score before submitting
        calculateScore(examId, db);
        
        // Update exam status và end_time
        std::string sql = 
            "UPDATE Exams e "
            "JOIN Quizzes q ON e.quiz_id = q.quiz_id "
            "SET e.status = 'submitted', "
            "    e.end_time = DATE_ADD(e.start_time, INTERVAL q.time_limit SECOND) "
            "WHERE e.exam_id = " + std::to_string(examId) + 
            " AND e.status = 'in_progress';";
        
        db->executeUpdate(sql);
        std::cout << "[SUBMIT_EXAM] Exam submitted: " << examId << std::endl;
    } catch (sql::SQLException& e) {
        std::cerr << "[SUBMIT_EXAM] SQL error: " << e.what() << std::endl;
    }
}

// Calculate score for an exam
void calculateScore(int examId, DbManager* db) {
    try {
        // Get all questions for this exam
        std::string getQuestionsSql = 
            "SELECT DISTINCT q.question_id "
            "FROM Questions q "
            "JOIN Exams e ON q.quiz_id = e.quiz_id "
            "WHERE e.exam_id = " + std::to_string(examId) + ";";
        
        sql::ResultSet* questionsRes = db->executeQuery(getQuestionsSql);
        if (!questionsRes) {
            std::cerr << "[CALCULATE_SCORE] Failed to get questions\n";
            return;
        }
        
        int totalQuestions = 0;
        int correctAnswers = 0;
        
        while (questionsRes->next()) {
            totalQuestions++;
            int questionId = questionsRes->getInt("question_id");
            
            // Get student's answer
            std::string getAnswerSql = 
                "SELECT chosen_answer FROM Exam_Answers "
                "WHERE exam_id = " + std::to_string(examId) + 
                " AND question_id = " + std::to_string(questionId) + ";";
            
            sql::ResultSet* answerRes = db->executeQuery(getAnswerSql);
            if (answerRes && answerRes->next()) {
                std::string chosenAnswer = answerRes->getString("chosen_answer");
                
                // Get correct answer
                std::string getCorrectSql = 
                    "SELECT answer_text FROM Answers "
                    "WHERE question_id = " + std::to_string(questionId) + 
                    " AND is_correct = 1;";
                
                sql::ResultSet* correctRes = db->executeQuery(getCorrectSql);
                if (correctRes && correctRes->next()) {
                    std::string correctAnswer = correctRes->getString("answer_text");
                    
                    // Compare answers (case-insensitive, trim spaces)
                    std::string chosen = chosenAnswer;
                    std::string correct = correctAnswer;
                    // Convert to uppercase for comparison
                    std::transform(chosen.begin(), chosen.end(), chosen.begin(), ::toupper);
                    std::transform(correct.begin(), correct.end(), correct.begin(), ::toupper);
                    
                    if (chosen == correct) {
                        correctAnswers++;
                    }
                }
                delete correctRes;
            }
            delete answerRes;
        }
        delete questionsRes;
        
        // Calculate score (0-10 scale)
        double score = 0.0;
        if (totalQuestions > 0) {
            score = (double(correctAnswers) / double(totalQuestions)) * 10.0;
        }
        
        // Update score in database
        std::string updateScoreSql = 
            "UPDATE Exams SET score = " + std::to_string(score) + 
            " WHERE exam_id = " + std::to_string(examId) + ";";
        
        db->executeUpdate(updateScoreSql);
        std::cout << "[CALCULATE_SCORE] Exam " << examId << ": " 
                  << correctAnswers << "/" << totalQuestions << " correct, score = " << score << std::endl;
        
    } catch (sql::SQLException& e) {
        std::cerr << "[CALCULATE_SCORE] SQL error: " << e.what() << std::endl;
    }
}

// Get list of question IDs for a quiz
std::vector<int> getQuestionsForQuiz(int quizId, DbManager* db) {
    std::vector<int> questionIds;
    try {
        std::string sql = 
            "SELECT question_id FROM Questions "
            "WHERE quiz_id = " + std::to_string(quizId) + 
            " ORDER BY question_id;";
        
        sql::ResultSet* res = db->executeQuery(sql);
        if (res) {
            while (res->next()) {
                questionIds.push_back(res->getInt("question_id"));
            }
        }
        delete res;
    } catch (sql::SQLException& e) {
        std::cerr << "[GET_QUESTIONS_FOR_QUIZ] SQL error: " << e.what() << std::endl;
    }
    return questionIds;
}

