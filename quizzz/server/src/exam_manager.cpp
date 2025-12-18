#include "../include/exam_manager.h"
#include "../include/db_manager.h"
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/resultset.h>
#include <vector>
#include <iostream>

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
int getRemainingTime(int examId, DbManager* db) {
    try {
        std::string sql = 
            "SELECT q.time_limit - TIMESTAMPDIFF(SECOND, e.start_time, NOW()) as remaining "
            "FROM Exams e "
            "JOIN Quizzes q ON e.quiz_id = q.quiz_id "
            "WHERE e.exam_id = " + std::to_string(examId) + 
            " AND e.status = 'in_progress';";
        
        sql::ResultSet* res = db->executeQuery(sql);
        if (res && res->next()) {
            int remaining = res->getInt("remaining");
            delete res;
            return remaining > 0 ? remaining : 0; // Trả về 0 nếu hết thời gian
        }
        delete res;
        return -1; // Exam không tồn tại hoặc đã submit
    } catch (sql::SQLException& e) {
        std::cerr << "[GET_REMAINING_TIME] SQL error: " << e.what() << std::endl;
        return -1;
    }
}

// Check and auto-submit expired exams
void checkExpiredExams(DbManager* db) {
    try {
        // Tìm các exam đã hết thời gian
        std::string findSql = 
            "SELECT e.exam_id, e.user_id, e.quiz_id "
            "FROM Exams e "
            "JOIN Quizzes q ON e.quiz_id = q.quiz_id "
            "WHERE e.status = 'in_progress' "
            "  AND NOW() >= DATE_ADD(e.start_time, INTERVAL q.time_limit SECOND);";
        
        sql::ResultSet* res = db->executeQuery(findSql);
        if (!res) return;
        
        std::vector<int> expiredExamIds;
        while (res->next()) {
            expiredExamIds.push_back(res->getInt("exam_id"));
        }
        delete res;
        
        // Auto-submit các exam đã hết thời gian
        for (int examId : expiredExamIds) {
            submitExam(examId, db);
            std::cout << "[TIMER] Auto-submitted expired exam: " << examId << std::endl;
        }
    } catch (sql::SQLException& e) {
        std::cerr << "[CHECK_EXPIRED_EXAMS] SQL error: " << e.what() << std::endl;
    }
}

// Submit exam manually or automatically
void submitExam(int examId, DbManager* db) {
    try {
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

