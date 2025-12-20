#include "../include/quiz_manager.h"
#include "../include/db_manager.h"
#include "../include/protocol_manager.h"
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/resultset.h>
#include <sstream>
#include <iostream>

// ==================================================
// QUIZZES
// ==================================================

// LIST_QUIZZES
void handleListQuizzes(int sock, DbManager *db) {
    std::string q =
        "SELECT quiz_id, title, question_count, time_limit, status "
        "FROM Quizzes;";

    try {
        sql::ResultSet *res = db->executeQuery(q);
        if (!res) {
            sendLine(sock, "LIST_QUIZZES_FAIL|reason=db_error");
            return;
        }

        // format đơn giản: QUIZZES|id:title(count,time,status);id2:...
        std::stringstream ss;
        ss << "QUIZZES|";
        bool first = true;
        while (res->next()) {
            if (!first) ss << ";";
            first = false;
            int id = res->getInt("quiz_id");
            std::string title = res->getString("title");
            int cnt = res->getInt("question_count");
            int t = res->getInt("time_limit");
            std::string status = res->getString("status");

            ss << id << ":" << title << "("
               << cnt << "Q," << t << "s," << status << ")";
        }
        delete res;

        sendLine(sock, ss.str());
    } catch (sql::SQLException &e) {
        std::cerr << "[LIST_QUIZZES] SQL error: " << e.what() << std::endl;
        sendLine(sock, "LIST_QUIZZES_FAIL|reason=sql_error");
    }
}

// ADD_QUIZ|title|desc|count|time|type|start_time|end_time
// Format: ADD_QUIZ|title|desc|count|time (normal exam)
//         ADD_QUIZ|title|desc|count|time|scheduled|start_time|end_time (scheduled exam)
void handleAddQuiz(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() < 5) {
        sendLine(sock, "ADD_QUIZ_FAIL|reason=bad_format");
        return;
    }

    std::string title = escapeSql(parts[1]);
    std::string desc  = escapeSql(parts[2]);

    int count = 0, timeLimit = 0;
    try {
        count = std::stoi(parts[3]);
        timeLimit = std::stoi(parts[4]);
    } catch (...) {
        sendLine(sock, "ADD_QUIZ_FAIL|reason=bad_number");
        return;
    }
    
    // Parse exam type (default: normal)
    std::string examType = "normal";
    std::string examStartTime = "NULL";
    std::string examEndTime = "NULL";
    
    if (parts.size() >= 6) {
        examType = parts[5]; // "normal" hoặc "scheduled"
        
        if (examType == "scheduled") {
            if (parts.size() < 8) {
                sendLine(sock, "ADD_QUIZ_FAIL|reason=missing_scheduled_time");
                return;
            }
            examStartTime = "'" + escapeSql(parts[6]) + "'"; // "2025-01-20 09:00:00"
            examEndTime = "'" + escapeSql(parts[7]) + "'";   // "2025-01-20 12:00:00"
        }
    }

    std::string insertSql =
        "INSERT INTO Quizzes (title, description, question_count, time_limit, "
        "status, creator_id, exam_type, exam_start_time, exam_end_time) VALUES ('" +
        title + "', '" + desc + "', " +
        std::to_string(count) + ", " + std::to_string(timeLimit) +
        ", 'not_started', 2, '" + examType + "', " +
        examStartTime + ", " + examEndTime + ");";

    std::cout << "[ADD_QUIZ] SQL: " << insertSql << std::endl;
    int newId = db->executeInsertAndGetId(insertSql);
    if (newId == 0) {
        sendLine(sock, "ADD_QUIZ_FAIL|reason=sql_error");
        return;
    }

    sendLine(sock, "ADD_QUIZ_OK|quizId=" + std::to_string(newId));
    std::cout << "[ADD_QUIZ] quiz added, id=" << newId << ", type=" << examType << std::endl;
}

// EDIT_QUIZ|quizId|title|desc|count|time
void handleEditQuiz(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 6) {
        sendLine(sock, "EDIT_QUIZ_FAIL|reason=bad_format");
        return;
    }

    int quizId, count, timeLimit;
    try {
        quizId = std::stoi(parts[1]);
        count  = std::stoi(parts[4]);
        timeLimit = std::stoi(parts[5]);
    } catch (...) {
        sendLine(sock, "EDIT_QUIZ_FAIL|reason=bad_number");
        return;
    }

    std::string title = escapeSql(parts[2]);
    std::string desc  = escapeSql(parts[3]);

    std::string q =
        "UPDATE Quizzes SET "
        "title='" + title + "', "
        "description='" + desc + "', "
        "question_count=" + std::to_string(count) + ", "
        "time_limit=" + std::to_string(timeLimit) +
        " WHERE quiz_id=" + std::to_string(quizId) + ";";

    try {
        db->executeUpdate(q);
        sendLine(sock, "EDIT_QUIZ_OK");
    } catch (...) {
        sendLine(sock, "EDIT_QUIZ_FAIL|reason=sql_error");
    }
}

// DELETE_QUIZ|quizId
void handleDeleteQuiz(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 2) {
        sendLine(sock, "DELETE_QUIZ_FAIL|reason=bad_format");
        return;
    }

    int quizId;
    try {
        quizId = std::stoi(parts[1]);
    } catch (...) {
        sendLine(sock, "DELETE_QUIZ_FAIL|reason=bad_number");
        return;
    }

    try {
        // Xoá theo thứ tự tránh lỗi FK
        // 1) đáp án của các câu hỏi thuộc quiz
        std::string q1 =
            "DELETE FROM Answers WHERE question_id IN "
            "(SELECT question_id FROM Questions WHERE quiz_id=" +
            std::to_string(quizId) + ");";
        db->executeUpdate(q1);

        // 2) exam_answers của các exam thuộc quiz
        std::string q2 =
            "DELETE FROM Exam_Answers WHERE exam_id IN "
            "(SELECT exam_id FROM Exams WHERE quiz_id=" +
            std::to_string(quizId) + ");";
        db->executeUpdate(q2);

        // 3) exams thuộc quiz
        std::string q3 =
            "DELETE FROM Exams WHERE quiz_id=" +
            std::to_string(quizId) + ";";
        db->executeUpdate(q3);

        // 4) questions
        std::string q4 =
            "DELETE FROM Questions WHERE quiz_id=" +
            std::to_string(quizId) + ";";
        db->executeUpdate(q4);

        // 5) quiz
        std::string q5 =
            "DELETE FROM Quizzes WHERE quiz_id=" +
            std::to_string(quizId) + ";";
        db->executeUpdate(q5);

        sendLine(sock, "DELETE_QUIZ_OK");
    } catch (sql::SQLException &e) {
        std::cerr << "[DELETE_QUIZ] SQL error: " << e.what() << std::endl;
        sendLine(sock, "DELETE_QUIZ_FAIL|reason=sql_error");
    }
}

