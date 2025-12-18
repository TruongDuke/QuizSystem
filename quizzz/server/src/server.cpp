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

// ==================================================
// LOGIN
// ==================================================

// message client gửi: LOGIN|username|password
bool handleLogin(int clientSock, DbManager *db, std::string &outRole, std::string &outSessionId) {
    std::string line = recvLine(clientSock);
    if (line.empty()) {
        std::cerr << "[LOGIN] client disconnected before login\n";
        return false;
    }

    auto parts = split(line, '|');
    if (parts.size() != 3 || parts[0] != "LOGIN") {
        sendLine(clientSock, "LOGIN_FAIL|reason=bad_format");
        return false;
    }

    std::string username = escapeSql(parts[1]);
    std::string password = escapeSql(parts[2]);

    std::string q =
        "SELECT * FROM Users "
        "WHERE username = '" + username +
        "' AND password = '" + password + "';";

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
        std::string role = res->getString("role");
        int userId = res->getInt("user_id");
        outRole = role;
        
        // Create session
        SessionManager& sessionMgr = SessionManager::getInstance();
        outSessionId = sessionMgr.createSession(userId, username, role);

        sendLine(clientSock, "LOGIN_OK|sessionId=" + outSessionId + "|role=" + role);
        std::cout << "[LOGIN] user " << username << " logged in as " << role << std::endl;
        ok = true;
    } else {
        sendLine(clientSock, "LOGIN_FAIL|reason=wrong_credentials");
    }

    delete res;
    return ok;
}

// ==================================================
// EXAMS
// ==================================================

// LIST_EXAMS
void handleListExams(int sock, DbManager *db) {
    std::string q =
        "SELECT exam_id, quiz_id, user_id, score, status "
        "FROM Exams;";

    try {
        sql::ResultSet *res = db->executeQuery(q);
        if (!res) {
            sendLine(sock, "LIST_EXAMS_FAIL|reason=db_error");
            return;
        }

        std::stringstream ss;
        ss << "EXAMS|";
        bool first = true;
        while (res->next()) {
            if (!first) ss << ";";
            first = false;
            int eid = res->getInt("exam_id");
            int qid = res->getInt("quiz_id");
            int uid = res->getInt("user_id");
            double score = res->getDouble("score");
            std::string status = res->getString("status");
            ss << eid << "(quiz=" << qid
               << ",user=" << uid
               << ",score=" << score
               << ",status=" << status << ")";
        }
        delete res;
        sendLine(sock, ss.str());
    } catch (sql::SQLException &e) {
        std::cerr << "[LIST_EXAMS] SQL error: " << e.what() << std::endl;
        sendLine(sock, "LIST_EXAMS_FAIL|reason=sql_error");
    }
}

// ==================================================
// MAIN
// ==================================================

int main() {
    DbManager *db = new DbManager("127.0.0.1", "root", "123456", "quizDB");
    std::cout << "[DB] Connected to quizDB\n";

    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        std::cerr << "[NET] Error creating socket\n";
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(9000);
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

    int clientSock = accept(serverSock, nullptr, nullptr);
    if (clientSock < 0) {
        std::cerr << "[NET] Accept failed\n";
        return 1;
    }

    std::string role, sessionId;
    if (!handleLogin(clientSock, db, role, sessionId)) {
        std::cerr << "[MAIN] login failed, closing\n";
        close(clientSock);
        close(serverSock);
        delete db;
        return 0;
    }

    // Vòng lặp command
    while (true) {
        std::string line = recvLine(clientSock);
        if (line.empty()) {
            std::cout << "[MAIN] client disconnected\n";
            break;
        }

        auto parts = split(line, '|');
        if (parts.empty()) continue;

        std::string cmd = parts[0];

        if (cmd == "LIST_QUIZZES") {
            handleListQuizzes(clientSock, db);

        } else if (cmd == "ADD_QUIZ") {
            if (role == "teacher")
                handleAddQuiz(parts, clientSock, db);
            else
                sendLine(clientSock, "ADD_QUIZ_FAIL|reason=permission_denied");

        } else if (cmd == "EDIT_QUIZ") {
            if (role == "teacher")
                handleEditQuiz(parts, clientSock, db);
            else
                sendLine(clientSock, "EDIT_QUIZ_FAIL|reason=permission_denied");

        } else if (cmd == "DELETE_QUIZ") {
            if (role == "teacher")
                handleDeleteQuiz(parts, clientSock, db);
            else
                sendLine(clientSock, "DELETE_QUIZ_FAIL|reason=permission_denied");

        } else if (cmd == "LIST_QUESTIONS") {
            handleListQuestions(parts, clientSock, db);

        } else if (cmd == "ADD_QUESTION") {
            if (role == "teacher")
                handleAddQuestion(parts, clientSock, db);
            else
                sendLine(clientSock, "ADD_QUESTION_FAIL|reason=permission_denied");

        } else if (cmd == "EDIT_QUESTION") {
            if (role == "teacher")
                handleEditQuestion(parts, clientSock, db);
            else
                sendLine(clientSock, "EDIT_QUESTION_FAIL|reason=permission_denied");

        } else if (cmd == "DELETE_QUESTION") {
            if (role == "teacher")
                handleDeleteQuestion(parts, clientSock, db);
            else
                sendLine(clientSock, "DELETE_QUESTION_FAIL|reason=permission_denied");

        } else if (cmd == "LIST_EXAMS") {
            handleListExams(clientSock, db);

        } else if (cmd == "QUIT") {
            sendLine(clientSock, "BYE");
            break;

        } else {
            sendLine(clientSock, "ERR|reason=unknown_command");
        }
    }

    close(clientSock);
    close(serverSock);
    delete db;
    return 0;
}

