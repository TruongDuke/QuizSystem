#include "../include/server.h"
#include "../include/db_manager.h"
#include "../include/protocol_manager.h"
#include "../include/session_manager.h"
#include "../include/quiz_manager.h"
#include "../include/question_manager.h"
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/resultset.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

// Helper: Gửi câu hỏi tiếp theo cho học sinh
void sendNextQuestion(int sock, DbManager *db) {
    SessionManager& sessionMgr = SessionManager::getInstance();
    StudentSession* s = sessionMgr.getSession(sock);
    if (!s) return;

    if (s->currentQuestionIdx >= s->questionIds.size()) {
        std::string endSql = "UPDATE Exams SET status='submitted', end_time=NOW(), score=" + 
            std::to_string(s->score) + " WHERE exam_id=" + std::to_string(s->examId);
        db->executeUpdate(endSql);
        sendLine(sock, "END_EXAM|" + std::to_string(s->score));
        sessionMgr.removeSession(sock);
        return;
    }

    int qId = s->questionIds[s->currentQuestionIdx];
    std::string qText;
    sql::ResultSet *resQ = db->executeQuery("SELECT question_text FROM Questions WHERE question_id=" + std::to_string(qId));
    if (resQ && resQ->next()) qText = resQ->getString("question_text");
    delete resQ;

    std::vector<std::string> answers;
    sql::ResultSet *resA = db->executeQuery("SELECT answer_text FROM Answers WHERE question_id=" + std::to_string(qId) + " LIMIT 4");
    while (resA && resA->next()) answers.push_back(resA->getString("answer_text"));
    delete resA;
    
    while (answers.size() < 4) answers.push_back("");

    std::string msg = "QUESTION|" + std::to_string(qId) + "|" + qText + "|" + 
        answers[0] + "|" + answers[1] + "|" + answers[2] + "|" + answers[3];
    sendLine(sock, msg);
}

bool handleLogin(int clientSock, DbManager *db, std::string &outRole, int &outUserId) {
    std::string line = recvLine(clientSock);
    if (line.empty()) {
        std::cerr << "[LOGIN] client disconnected\n";
        return false;
    }
    auto parts = split(line, '|');
    if (parts.size() != 3 || parts[0] != "LOGIN") {
        sendLine(clientSock, "LOGIN_FAIL|reason=bad_format");
        return false;
    }
    std::string username = escapeSql(parts[1]);
    std::string password = escapeSql(parts[2]);
    std::string q = "SELECT user_id, role FROM Users WHERE username = '" + username + "' AND password = '" + password + "';";
    sql::ResultSet *res = nullptr;
    try {
        res = db->executeQuery(q);
    } catch (sql::SQLException &e) {
        std::cerr << "[LOGIN] SQL error: " << e.what() << std::endl;
        sendLine(clientSock, "LOGIN_FAIL|reason=sql_error");
        return false;
    }
    bool ok = false;
    if (res && res->next()) {
        outRole = res->getString("role");
        outUserId = res->getInt("user_id");
        sendLine(clientSock, "LOGIN_OK|sessionId=S123|role=" + outRole);
        std::cout << "[LOGIN] user " << username << " (ID:" << outUserId << ") logged in as " << outRole << std::endl;
        ok = true;
    } else {
        sendLine(clientSock, "LOGIN_FAIL|reason=wrong_credentials");
    }
    delete res;
    return ok;
}

void handleListExams(int sock, DbManager *db) {
    std::string q = "SELECT exam_id, quiz_id, user_id, score, status FROM Exams;";
    try {
        sql::ResultSet *res = db->executeQuery(q);
        std::stringstream ss;
        ss << "EXAMS|";
        bool first = true;
        while (res && res->next()) {
            if (!first) ss << ";";
            first = false;
            ss << res->getInt("exam_id") << "(quiz=" << res->getInt("quiz_id") << ",user=" << 
                res->getInt("user_id") << ",score=" << res->getDouble("score") << ",status=" << res->getString("status") << ")";
        }
        delete res;
        sendLine(sock, ss.str());
    } catch (...) {
        sendLine(sock, "LIST_EXAMS_FAIL|reason=sql_error");
    }
}

void handleStudentJoinExam(const std::vector<std::string> &parts, int sock, int userId, DbManager *db) {
    if (parts.size() < 2) return;
    int quizId = std::stoi(parts[1]);
    try {
        std::string sql = "INSERT INTO Exams (quiz_id, user_id, start_time, score, status) VALUES (" + 
            std::to_string(quizId) + ", " + std::to_string(userId) + ", NOW(), 0, 'in_progress')";
        int examId = db->executeInsertAndGetId(sql);
        std::vector<int> qIds;
        sql::ResultSet *res = db->executeQuery("SELECT question_id FROM Questions WHERE quiz_id=" + std::to_string(quizId));
        while (res && res->next()) qIds.push_back(res->getInt("question_id"));
        delete res;
        if (qIds.empty()) {
            sendLine(sock, "JOIN_FAIL|Empty_Quiz");
            return;
        }
        StudentSession s;
        s.examId = examId;
        s.currentQuestionIdx = 0;
        s.score = 0;
        s.questionIds = qIds;
        SessionManager& sessionMgr = SessionManager::getInstance();
        sessionMgr.addSession(sock, s);
        sendLine(sock, "TEST_STARTED");
        sendNextQuestion(sock, db);
    } catch (sql::SQLException &e) {
        std::cerr << "[JOIN] SQL Error: " << e.what() << std::endl;
        sendLine(sock, "JOIN_FAIL|DB_Error");
    }
}

void handleStudentAnswer(const std::vector<std::string> &parts, int sock, DbManager *db) {
    SessionManager& sessionMgr = SessionManager::getInstance();
    StudentSession* s = sessionMgr.getSession(sock);
    if (!s || parts.size() < 3) return;
    int qId = std::stoi(parts[1]);
    std::string choiceChar = parts[2];
    int choiceIdx = 0;
    if (choiceChar == "B") choiceIdx = 1;
    else if (choiceChar == "C") choiceIdx = 2;
    else if (choiceChar == "D") choiceIdx = 3;
    std::string sql = "SELECT answer_id, answer_text, is_correct FROM Answers WHERE question_id=" + 
        std::to_string(qId) + " LIMIT 1 OFFSET " + std::to_string(choiceIdx);
    try {
        sql::ResultSet *res = db->executeQuery(sql);
        if (res && res->next()) {
            int ansId = res->getInt("answer_id");
            std::string ansText = res->getString("answer_text");
            bool isCorrect = res->getBoolean("is_correct");
            std::string ins = "INSERT INTO Exam_Answers (exam_id, question_id, answer_id, chosen_answer) VALUES (" +
                std::to_string(s->examId) + ", " + std::to_string(qId) + ", " + std::to_string(ansId) + ", '" + escapeSql(ansText) + "')";
            db->executeUpdate(ins);
            if (isCorrect) {
                s->score += 1.0;
                db->executeUpdate("UPDATE Exams SET score=" + std::to_string(s->score) + " WHERE exam_id=" + std::to_string(s->examId));
            }
        }
        if(res) delete res;
    } catch (...) {}
    s->currentQuestionIdx++;
    sendNextQuestion(sock, db);
}

void handleListHistory(int sock, int userId, DbManager *db) {
    std::string sql = "SELECT e.start_time, q.title, e.score, e.status FROM Exams e JOIN Quizzes q ON e.quiz_id = q.quiz_id WHERE e.user_id=" + std::to_string(userId);
    try {
        sql::ResultSet *res = db->executeQuery(sql);
        std::stringstream ss;
        ss << "HISTORY|";
        bool first = true;
        while (res && res->next()) {
            if (!first) ss << ";";
            first = false;
            ss << "[" << res->getString("start_time") << "] " << res->getString("title") << " - Score: " << res->getDouble("score");
        }
        delete res;
        sendLine(sock, ss.str());
    } catch (...) {
        sendLine(sock, "HISTORY_FAIL|reason=sql_error");
    }
}

int main() {
    DbManager *db = new DbManager("127.0.0.1", "root", "123456", "quizDB");
    std::cout << "[DB] Connected to quizDB\n";
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        std::cerr << "[NET] Error creating socket\n";
        return 1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[NET] Bind failed\n";
        return 1;
    }
    if (listen(serverSock, 5) < 0) {
        std::cerr << "[NET] Listen failed\n";
        return 1;
    }
    std::cout << "[NET] Server is listening on port 9000...\n";
    while (true) {
        int clientSock = accept(serverSock, nullptr, nullptr);
        if (clientSock < 0) {
            std::cerr << "[NET] Accept failed\n";
            continue;
        }
        std::cout << "[NET] Client connected.\n";
        std::string role;
        int userId = -1;
        if (!handleLogin(clientSock, db, role, userId)) {
            std::cerr << "[MAIN] Login failed\n";
            close(clientSock);
            continue;
        }
        while (true) {
            std::string line = recvLine(clientSock);
            if (line.empty()) {
                std::cout << "[MAIN] Client disconnected\n";
                break;
            }
            auto parts = split(line, '|');
            if (parts.empty()) continue;
            std::string cmd = parts[0];
            if (cmd == "QUIT") {
                sendLine(clientSock, "BYE");
                break;
            }
            if (role == "teacher") {
                if (cmd == "LIST_QUIZZES") handleListQuizzes(clientSock, db);
                else if (cmd == "ADD_QUIZ") handleAddQuiz(parts, clientSock, db);
                else if (cmd == "EDIT_QUIZ") handleEditQuiz(parts, clientSock, db);
                else if (cmd == "DELETE_QUIZ") handleDeleteQuiz(parts, clientSock, db);
                else if (cmd == "LIST_QUESTIONS") handleListQuestions(parts, clientSock, db);
                else if (cmd == "ADD_QUESTION") handleAddQuestion(parts, clientSock, db);
                else if (cmd == "EDIT_QUESTION") handleEditQuestion(parts, clientSock, db);
                else if (cmd == "DELETE_QUESTION") handleDeleteQuestion(parts, clientSock, db);
                else if (cmd == "LIST_EXAMS") handleListExams(clientSock, db);
                else sendLine(clientSock, "ERR|unknown_command");
            } else if (role == "student") {
                if (cmd == "LIST_QUIZZES") handleListQuizzes(clientSock, db);
                else if (cmd == "JOIN_ROOM") handleStudentJoinExam(parts, clientSock, userId, db);
                else if (cmd == "ANSWER") handleStudentAnswer(parts, clientSock, db);
                else if (cmd == "LIST_MY_HISTORY") handleListHistory(clientSock, userId, db);
                else sendLine(clientSock, "ERR|unknown_command");
            } else {
                sendLine(clientSock, "ERR|permission_denied");
            }
        }
        SessionManager& sessionMgr = SessionManager::getInstance();
        if (sessionMgr.getSession(clientSock)) {
            sessionMgr.removeSession(clientSock);
        }
        close(clientSock);
    }
    close(serverSock);
    delete db;
    return 0;
}

