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
void handleListQuizzes(int sock, DbManager *db, const std::string& role) {
    // Auto-update scheduled quiz status first
    updateScheduledQuizStatus(db);
    
    std::string q =
        "SELECT quiz_id, title, question_count, time_limit, status, "
        "exam_type, exam_start_time, exam_end_time "
        "FROM Quizzes;";

    try {
        sql::ResultSet *res = db->executeQuery(q);
        if (!res) {
            sendLine(sock, "LIST_QUIZZES_FAIL|reason=db_error");
            return;
        }

        // format: QUIZZES|id:title(count,time,status,type,start,end,actual);id2:...
        // actual is only included for teachers
        std::stringstream ss;
        ss << "QUIZZES|";
        bool first = true;
        while (res->next()) {
            int id = res->getInt("quiz_id");
            std::string title = res->getString("title");
            int cnt = res->getInt("question_count");
            int t = res->getInt("time_limit");
            std::string status = res->getString("status");
            std::string examType = res->getString("exam_type");
            std::string startTime = res->isNull("exam_start_time") ? "NULL" : res->getString("exam_start_time");
            std::string endTime = res->isNull("exam_end_time") ? "NULL" : res->getString("exam_end_time");

            // Count actual questions in this quiz
            std::string countQuery = "SELECT COUNT(*) as actual_count FROM Questions WHERE quiz_id=" + 
                                    std::to_string(id) + ";";
            sql::ResultSet *countRes = db->executeQuery(countQuery);
            int actualCount = 0;
            if (countRes && countRes->next()) {
                actualCount = countRes->getInt("actual_count");
            }
            if (countRes) delete countRes;
            
            // Filter for students: only show quizzes with enough questions
            if (role == "student") {
                // Skip if not enough questions
                if (actualCount < cnt) {
                    continue;
                }
            }

            if (!first) ss << ";";
            first = false;
            ss << id << ":" << title << "("
               << cnt << "Q," << t << "s," << status << ","
               << examType << "," << startTime << "," << endTime;
            
            // Add actual count for teachers
            if (role == "teacher" || role == "admin") {
                ss << "," << actualCount;
            }
            
            ss << ")";
        }
        delete res;

        sendLine(sock, ss.str());
    } catch (sql::SQLException &e) {
        std::cerr << "[LIST_QUIZZES] SQL error: " << e.what() << std::endl;
        sendLine(sock, "LIST_QUIZZES_FAIL|reason=sql_error");
    }
}

// ADD_QUIZ|title|desc|count|time|type|start_datetime|end_datetime
// Format: ADD_QUIZ|title|desc|count|time (normal exam)
//         ADD_QUIZ|title|desc|count|time|scheduled|YYYY-MM-DD HH:MM|YYYY-MM-DD HH:MM (scheduled exam)
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
    std::string defaultStatus = "in_progress"; // Normal quiz sẵn sàng thi ngay
    
    if (parts.size() >= 6) {
        examType = parts[5]; // "normal" hoặc "scheduled"
        
        if (examType == "scheduled") {
            if (parts.size() < 8) {
                sendLine(sock, "ADD_QUIZ_FAIL|reason=missing_scheduled_time");
                return;
            }
            
            // Parse datetime strings: YYYY-MM-DD HH:MM
            std::string startDateTime = parts[6];
            std::string endDateTime = parts[7];
            
            // Validate format (basic check)
            if (startDateTime.length() < 16 || endDateTime.length() < 16) {
                sendLine(sock, "ADD_QUIZ_FAIL|reason=bad_datetime_format");
                return;
            }
            
            examStartTime = "'" + startDateTime + ":00'";
            examEndTime = "'" + endDateTime + ":00'";
            defaultStatus = "not_started"; // Scheduled quiz chờ đến thời gian
        }
    }

    std::string insertSql =
        "INSERT INTO Quizzes (title, description, question_count, time_limit, "
        "status, creator_id, exam_type, exam_start_time, exam_end_time) VALUES ('" +
        title + "', '" + desc + "', " +
        std::to_string(count) + ", " + std::to_string(timeLimit) +
        ", '" + defaultStatus + "', 2, '" + examType + "', " +
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

// EDIT_QUIZ|quizId|title|desc|count|time|type|start_datetime|end_datetime
// Format: EDIT_QUIZ|quizId|title|desc|count|time (normal exam)
//         EDIT_QUIZ|quizId|title|desc|count|time|normal (explicitly normal)
//         EDIT_QUIZ|quizId|title|desc|count|time|scheduled|YYYY-MM-DD HH:MM|YYYY-MM-DD HH:MM (scheduled exam)
void handleEditQuiz(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() < 6) {
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
    
    // Parse exam type (default: normal)
    std::string examType = "normal";
    std::string examStartTime = "NULL";
    std::string examEndTime = "NULL";
    
    if (parts.size() >= 7) {
        examType = parts[6]; // "normal" hoặc "scheduled"
        
        if (examType == "scheduled") {
            if (parts.size() < 9) {
                sendLine(sock, "EDIT_QUIZ_FAIL|reason=missing_scheduled_time");
                return;
            }
            
            // Parse datetime strings: YYYY-MM-DD HH:MM
            std::string startDateTime = parts[7];
            std::string endDateTime = parts[8];
            
            // Validate format (basic check)
            if (startDateTime.length() < 16 || endDateTime.length() < 16) {
                sendLine(sock, "EDIT_QUIZ_FAIL|reason=bad_datetime_format");
                return;
            }
            
            examStartTime = "'" + startDateTime + ":00'";
            examEndTime = "'" + endDateTime + ":00'";
        }
    }

    std::string q =
        "UPDATE Quizzes SET "
        "title='" + title + "', "
        "description='" + desc + "', "
        "question_count=" + std::to_string(count) + ", "
        "time_limit=" + std::to_string(timeLimit) + ", "
        "exam_type='" + examType + "', "
        "exam_start_time=" + examStartTime + ", "
        "exam_end_time=" + examEndTime +
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

// Auto-update scheduled quiz status based on current time
void updateScheduledQuizStatus(DbManager *db) {
    try {
        // Update to 'in_progress' if current time is between start and end time
        std::string q1 = 
            "UPDATE Quizzes SET status='in_progress' "
            "WHERE exam_type='scheduled' AND status='not_started' "
            "AND NOW() >= exam_start_time AND NOW() < exam_end_time;";
        db->executeUpdate(q1);
        
        // Update to 'finished' if current time is past end time
        std::string q2 = 
            "UPDATE Quizzes SET status='finished' "
            "WHERE exam_type='scheduled' AND status IN ('not_started', 'in_progress') "
            "AND NOW() >= exam_end_time;";
        db->executeUpdate(q2);
    } catch (sql::SQLException &e) {
        std::cerr << "[UPDATE_SCHEDULED] SQL error: " << e.what() << std::endl;
    }
}

// STATUS_QUIZ|quizId|newStatus
void handleStatusQuiz(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 3) {
        sendLine(sock, "STATUS_QUIZ_FAIL|reason=bad_format");
        return;
    }
    
    int quizId;
    try {
        quizId = std::stoi(parts[1]);
    } catch (...) {
        sendLine(sock, "STATUS_QUIZ_FAIL|reason=bad_number");
        return;
    }
    
    std::string newStatus = escapeSql(parts[2]);
    
    // Validate status
    if (newStatus != "not_started" && newStatus != "in_progress" && newStatus != "finished") {
        sendLine(sock, "STATUS_QUIZ_FAIL|reason=invalid_status");
        return;
    }
    
    try {
        std::string q = 
            "UPDATE Quizzes SET status='" + newStatus + "' "
            "WHERE quiz_id=" + std::to_string(quizId) + ";";
        db->executeUpdate(q);
        sendLine(sock, "STATUS_QUIZ_OK");
        std::cout << "[STATUS_QUIZ] Quiz " << quizId << " status changed to " << newStatus << std::endl;
    } catch (sql::SQLException &e) {
        std::cerr << "[STATUS_QUIZ] SQL error: " << e.what() << std::endl;
        sendLine(sock, "STATUS_QUIZ_FAIL|reason=sql_error");
    }
}

