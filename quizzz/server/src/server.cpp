#include "../include/server.h"
#include "../include/db_manager.h"
#include "../include/protocol_manager.h"
#include "../include/session_manager.h"
#include "../include/quiz_manager.h"
#include "../include/question_manager.h"
#include "../include/exam_manager.h"
#include "../include/client_manager.h"
#include "../include/broadcast_manager.h"
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/resultset.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <chrono>
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
// Command Handler
// ==================================================

void handleCommand(int clientSock, const std::vector<std::string>& parts, 
                  DbManager* db, ClientManager& clientMgr) {
    if (parts.empty()) return;
    
    ClientInfo* clientInfo = clientMgr.getClient(clientSock);
    if (!clientInfo || !clientInfo->authenticated) {
        sendLine(clientSock, "ERR|reason=not_authenticated");
        return;
    }
    
    std::string cmd = parts[0];
    std::string role = clientInfo->role;
    
    if (cmd == "LIST_QUIZZES") {
        handleListQuizzes(clientSock, db);
        
    } else if (cmd == "ADD_QUIZ") {
        if (role == "teacher") {
            handleAddQuiz(parts, clientSock, db);
            // Broadcast new quiz to all students
            BroadcastManager& bcast = BroadcastManager::getInstance();
            bcast.broadcastToRole("student", "NOTIFICATION|new_quiz_available");
        } else {
            sendLine(clientSock, "ADD_QUIZ_FAIL|reason=permission_denied");
        }
        
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
        
    } else if (cmd == "START_EXAM") {
        if (parts.size() != 2) {
            sendLine(clientSock, "START_EXAM_FAIL|reason=bad_format");
            return;
        }
        int quizId;
        try {
            quizId = std::stoi(parts[1]);
        } catch (...) {
            sendLine(clientSock, "START_EXAM_FAIL|reason=bad_number");
            return;
        }
        int examId = startExam(clientInfo->userId, quizId, db);
        if (examId > 0) {
            sendLine(clientSock, "START_EXAM_OK|examId=" + std::to_string(examId));
        } else {
            sendLine(clientSock, "START_EXAM_FAIL|reason=sql_error");
        }
        
    } else if (cmd == "GET_REMAINING_TIME") {
        if (parts.size() != 2) {
            sendLine(clientSock, "REMAINING_TIME_FAIL|reason=bad_format");
            return;
        }
        int examId;
        try {
            examId = std::stoi(parts[1]);
        } catch (...) {
            sendLine(clientSock, "REMAINING_TIME_FAIL|reason=bad_number");
            return;
        }
        int remaining = getRemainingTime(examId, db);
        if (remaining >= 0) {
            sendLine(clientSock, "REMAINING_TIME|seconds=" + std::to_string(remaining));
        } else {
            sendLine(clientSock, "REMAINING_TIME_FAIL|reason=exam_not_found");
        }
        
    } else if (cmd == "SUBMIT_EXAM") {
        if (parts.size() != 2) {
            sendLine(clientSock, "SUBMIT_EXAM_FAIL|reason=bad_format");
            return;
        }
        int examId;
        try {
            examId = std::stoi(parts[1]);
        } catch (...) {
            sendLine(clientSock, "SUBMIT_EXAM_FAIL|reason=bad_number");
            return;
        }
        submitExam(examId, db);
        sendLine(clientSock, "SUBMIT_EXAM_OK");
        
    } else if (cmd == "QUIT") {
        sendLine(clientSock, "BYE");
        
    } else {
        sendLine(clientSock, "ERR|reason=unknown_command");
    }
}

// ==================================================
// Timer Thread - Check expired exams
// ==================================================

void timerThread(DbManager* db) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        checkExpiredExams(db);
    }
}

// ==================================================
// MAIN với Async I/O
// ==================================================

int main() {
    DbManager *db = new DbManager("127.0.0.1", "root", "123456", "quizDB");
    std::cout << "[DB] Connected to quizDB\n";
    
    // Start timer thread to check expired exams
    std::thread timer(timerThread, db);
    timer.detach(); // Run in background
    std::cout << "[TIMER] Timer thread started - checking expired exams every 5 seconds\n";
    
    ClientManager& clientMgr = ClientManager::getInstance();
    
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        std::cerr << "[NET] Error creating socket\n";
        return 1;
    }
    
    // Set socket to reuse address
    int opt = 1;
    setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
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
    std::cout << "[NET] Using async I/O with select() - Multiple clients supported\n";
    
    fd_set master_set, read_set;
    int max_fd = serverSock;
    
    FD_ZERO(&master_set);
    FD_SET(serverSock, &master_set);
    
    while (true) {
        read_set = master_set;
        
        // Select with timeout (1 second) - NON-BLOCKING
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(max_fd + 1, &read_set, nullptr, nullptr, &timeout);
        
        if (activity < 0) {
            std::cerr << "[NET] Select error\n";
            break;
        }
        
        // Check for new connections
        if (FD_ISSET(serverSock, &read_set)) {
            sockaddr_in clientAddr;
            socklen_t addrLen = sizeof(clientAddr);
            int clientSock = accept(serverSock, (sockaddr*)&clientAddr, &addrLen);
            
            if (clientSock >= 0) {
                std::cout << "[NET] New client connected: " << clientSock << std::endl;
                clientMgr.addClient(clientSock);
                FD_SET(clientSock, &master_set);
                if (clientSock > max_fd) max_fd = clientSock;
            }
        }
        
        // Check existing clients
        std::vector<int> clients = clientMgr.getAllClients();
        for (int clientSock : clients) {
            if (FD_ISSET(clientSock, &read_set)) {
                ClientInfo* info = clientMgr.getClient(clientSock);
                
                // Handle login for unauthenticated clients
                if (!info || !info->authenticated) {
                    std::string role, sessionId;
                    if (handleLogin(clientSock, db, role, sessionId)) {
                        // Get user info from session
                        SessionManager& sessionMgr = SessionManager::getInstance();
                        Session* session = sessionMgr.getSession(sessionId);
                        if (session) {
                            clientMgr.setClientInfo(clientSock, sessionId, role, 
                                                   session->username, session->userId);
                            std::cout << "[NET] Client " << clientSock 
                                     << " authenticated as " << role << std::endl;
                        }
                    } else {
                        // Login failed, close connection
                        std::cout << "[NET] Client " << clientSock << " login failed\n";
                        close(clientSock);
                        FD_CLR(clientSock, &master_set);
                        clientMgr.removeClient(clientSock);
                        continue;
                    }
                } else {
                    // Handle commands for authenticated clients
                    std::string line = recvLine(clientSock);
                    if (line.empty()) {
                        std::cout << "[NET] Client " << clientSock << " disconnected\n";
                        close(clientSock);
                        FD_CLR(clientSock, &master_set);
                        clientMgr.removeClient(clientSock);
                        continue;
                    }
                    
                    auto parts = split(line, '|');
                    handleCommand(clientSock, parts, db, clientMgr);
                    
                    // Check if client wants to quit
                    if (!parts.empty() && parts[0] == "QUIT") {
                        std::cout << "[NET] Client " << clientSock << " requested quit\n";
                        close(clientSock);
                        FD_CLR(clientSock, &master_set);
                        clientMgr.removeClient(clientSock);
                    }
                }
            }
        }
    }
    
    // Cleanup
    std::vector<int> allClients = clientMgr.getAllClients();
    for (int sock : allClients) {
        close(sock);
    }
    close(serverSock);
    delete db;
    return 0;
}

