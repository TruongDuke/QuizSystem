#include "../include/question_manager.h"
#include "../include/db_manager.h"
#include "../include/protocol_manager.h"
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/resultset.h>
#include <sstream>
#include <iostream>

// ==================================================
// QUESTIONS
// ==================================================

// LIST_QUESTIONS|quizId
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

    std::string q =
        "SELECT question_id, question_text "
        "FROM Questions WHERE quiz_id=" + std::to_string(quizId) + ";";

    try {
        sql::ResultSet *res = db->executeQuery(q);
        if (!res) {
            sendLine(sock, "LIST_QUESTIONS_FAIL|reason=db_error");
            return;
        }

        std::stringstream ss;
        ss << "QUESTIONS|" << quizId << "|";
        bool first = true;
        while (res->next()) {
            if (!first) ss << ";";
            first = false;
            int qid = res->getInt("question_id");
            std::string text = res->getString("question_text");
            ss << qid << ":" << text;
        }
        delete res;
        sendLine(sock, ss.str());
    } catch (sql::SQLException &e) {
        std::cerr << "[LIST_QUESTIONS] SQL error: " << e.what() << std::endl;
        sendLine(sock, "LIST_QUESTIONS_FAIL|reason=sql_error");
    }
}

// ADD_QUESTION|quizId|content|opt1|opt2|opt3|opt4|correctIndex
void handleAddQuestion(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 8) {
        sendLine(sock, "ADD_QUESTION_FAIL|reason=bad_format");
        return;
    }

    int quizId, correct;
    try {
        quizId  = std::stoi(parts[1]);
        correct = std::stoi(parts[7]); // 1..4
    } catch (...) {
        sendLine(sock, "ADD_QUESTION_FAIL|reason=bad_number");
        return;
    }

    std::string content = escapeSql(parts[2]);
    std::string o1 = escapeSql(parts[3]);
    std::string o2 = escapeSql(parts[4]);
    std::string o3 = escapeSql(parts[5]);
    std::string o4 = escapeSql(parts[6]);

    try {
        // 1) Questions
        std::string qInsert =
            "INSERT INTO Questions (quiz_id, question_text, difficulty, topic) "
            "VALUES (" + std::to_string(quizId) + ", '" + content +
            "', 'easy', 'general');";

        int questionId = db->executeInsertAndGetId(qInsert);
        if (questionId == 0) {
            sendLine(sock, "ADD_QUESTION_FAIL|reason=sql_error");
            return;
        }

        // 2) Answers
        auto insertAns = [&](const std::string &txt, int index) {
            std::string a =
                "INSERT INTO Answers (question_id, answer_text, is_correct) "
                "VALUES (" + std::to_string(questionId) + ", '" +
                escapeSql(txt) + "', " + (index == correct ? "1" : "0") + ");";
            db->executeUpdate(a);
        };

        insertAns(o1, 1);
        insertAns(o2, 2);
        insertAns(o3, 3);
        insertAns(o4, 4);

        sendLine(sock, "ADD_QUESTION_OK");
        std::cout << "[ADD_QUESTION] question added for quiz " << quizId
             << ", qid=" << questionId << std::endl;

    } catch (sql::SQLException &e) {
        std::cerr << "[ADD_QUESTION] SQL error: " << e.what() << std::endl;
        sendLine(sock, "ADD_QUESTION_FAIL|reason=sql_error");
    }
}

// EDIT_QUESTION|qId|content|opt1|opt2|opt3|opt4|correctIndex
void handleEditQuestion(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 8) {
        sendLine(sock, "EDIT_QUESTION_FAIL|reason=bad_format");
        return;
    }

    int qid, correct;
    try {
        qid     = std::stoi(parts[1]);
        correct = std::stoi(parts[7]);
    } catch (...) {
        sendLine(sock, "EDIT_QUESTION_FAIL|reason=bad_number");
        return;
    }

    std::string content = escapeSql(parts[2]);
    std::string o1 = escapeSql(parts[3]);
    std::string o2 = escapeSql(parts[4]);
    std::string o3 = escapeSql(parts[5]);
    std::string o4 = escapeSql(parts[6]);

    try {
        // update question text
        std::string q =
            "UPDATE Questions SET question_text='" + content +
            "' WHERE question_id=" + std::to_string(qid) + ";";
        db->executeUpdate(q);

        // xoá toàn bộ answers cũ
        std::string d =
            "DELETE FROM Answers WHERE question_id=" +
            std::to_string(qid) + ";";
        db->executeUpdate(d);

        // insert lại 4 đáp án
        auto insertAns = [&](const std::string &txt, int index) {
            std::string a =
                "INSERT INTO Answers (question_id, answer_text, is_correct) "
                "VALUES (" + std::to_string(qid) + ", '" +
                escapeSql(txt) + "', " + (index == correct ? "1" : "0") + ");";
            db->executeUpdate(a);
        };

        insertAns(o1, 1);
        insertAns(o2, 2);
        insertAns(o3, 3);
        insertAns(o4, 4);

        sendLine(sock, "EDIT_QUESTION_OK");
    } catch (sql::SQLException &e) {
        std::cerr << "[EDIT_QUESTION] SQL error: " << e.what() << std::endl;
        sendLine(sock, "EDIT_QUESTION_FAIL|reason=sql_error");
    }
}

// DELETE_QUESTION|qId
void handleDeleteQuestion(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 2) {
        sendLine(sock, "DELETE_QUESTION_FAIL|reason=bad_format");
        return;
    }
    int qid;
    try {
        qid = std::stoi(parts[1]);
    } catch (...) {
        sendLine(sock, "DELETE_QUESTION_FAIL|reason=bad_number");
        return;
    }

    try {
        std::string dAns =
            "DELETE FROM Answers WHERE question_id=" +
            std::to_string(qid) + ";";
        db->executeUpdate(dAns);

        std::string dQ =
            "DELETE FROM Questions WHERE question_id=" +
            std::to_string(qid) + ";";
        db->executeUpdate(dQ);

        sendLine(sock, "DELETE_QUESTION_OK");
    } catch (sql::SQLException &e) {
        std::cerr << "[DELETE_QUESTION] SQL error: " << e.what() << std::endl;
        sendLine(sock, "DELETE_QUESTION_FAIL|reason=sql_error");
    }
}

