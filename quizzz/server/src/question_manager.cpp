#include "../include/question_manager.h"
#include "../include/db_manager.h"
#include "../include/protocol_manager.h"
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/resultset.h>
#include <sstream>
#include <iostream>

void handleListQuestions(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 2) {
        sendLine(sock, "LIST_QUESTIONS_FAIL|reason=bad_format");
        return;
    }
    int quizId;
    try {
        quizId = std::stoi(parts[1]);
    } catch (...) {
        sendLine(sock, "LIST_QUESTIONS_FAIL|reason=bad_number");
        return;
    }
    std::string q = "SELECT question_id, question_text FROM Questions WHERE quiz_id=" + std::to_string(quizId) + ";";
    try {
        sql::ResultSet *res = db->executeQuery(q);
        std::stringstream ss;
        ss << "QUESTIONS|" << quizId << "|";
        bool first = true;
        while (res && res->next()) {
            if (!first) ss << ";";
            first = false;
            ss << res->getInt("question_id") << ":" << res->getString("question_text");
        }
        delete res;
        sendLine(sock, ss.str());
    } catch (...) {
        sendLine(sock, "LIST_QUESTIONS_FAIL|reason=sql_error");
    }
}

void handleAddQuestion(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 8) {
        sendLine(sock, "ADD_QUESTION_FAIL|reason=bad_format");
        return;
    }
    int quizId = std::stoi(parts[1]);
    int correct = std::stoi(parts[7]);
    std::string content = escapeSql(parts[2]);
    try {
        std::string qInsert = "INSERT INTO Questions (quiz_id, question_text, difficulty, topic) VALUES (" + 
            std::to_string(quizId) + ", '" + content + "', 'easy', 'general');";
        int questionId = db->executeInsertAndGetId(qInsert);
        auto insertAns = [&](const std::string &txt, int index) {
            std::string a = "INSERT INTO Answers (question_id, answer_text, is_correct) VALUES (" + 
                std::to_string(questionId) + ", '" + escapeSql(txt) + "', " + (index == correct ? "1" : "0") + ");";
            db->executeUpdate(a);
        };
        insertAns(parts[3], 1);
        insertAns(parts[4], 2);
        insertAns(parts[5], 3);
        insertAns(parts[6], 4);
        sendLine(sock, "ADD_QUESTION_OK");
    } catch (...) {
        sendLine(sock, "ADD_QUESTION_FAIL|reason=sql_error");
    }
}

void handleEditQuestion(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 8) {
        sendLine(sock, "EDIT_QUESTION_FAIL|reason=bad_format");
        return;
    }
    int qid = std::stoi(parts[1]);
    int correct = std::stoi(parts[7]);
    try {
        db->executeUpdate("UPDATE Questions SET question_text='" + escapeSql(parts[2]) + "' WHERE question_id=" + std::to_string(qid));
        db->executeUpdate("DELETE FROM Answers WHERE question_id=" + std::to_string(qid));
        auto insertAns = [&](const std::string &txt, int index) {
            std::string a = "INSERT INTO Answers (question_id, answer_text, is_correct) VALUES (" + 
                std::to_string(qid) + ", '" + escapeSql(txt) + "', " + (index == correct ? "1" : "0") + ");";
            db->executeUpdate(a);
        };
        insertAns(parts[3], 1);
        insertAns(parts[4], 2);
        insertAns(parts[5], 3);
        insertAns(parts[6], 4);
        sendLine(sock, "EDIT_QUESTION_OK");
    } catch (...) {
        sendLine(sock, "EDIT_QUESTION_FAIL|reason=sql_error");
    }
}

void handleDeleteQuestion(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 2) {
        sendLine(sock, "DELETE_QUESTION_FAIL|reason=bad_format");
        return;
    }
    int qid = std::stoi(parts[1]);
    try {
        db->executeUpdate("DELETE FROM Answers WHERE question_id=" + std::to_string(qid));
        db->executeUpdate("DELETE FROM Questions WHERE question_id=" + std::to_string(qid));
        sendLine(sock, "DELETE_QUESTION_OK");
    } catch (...) {
        sendLine(sock, "DELETE_QUESTION_FAIL|reason=sql_error");
    }
}

