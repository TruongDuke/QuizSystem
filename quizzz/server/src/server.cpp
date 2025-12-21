#include "../include/server.h"
#include "../include/db_manager.h"
#include "../include/protocol_manager.h"
#include "../include/session_manager.h"
#include "../include/quiz_manager.h"
#include "../include/question_manager.h"
#include "../include/exam_manager.h"
#include "../include/client_manager.h"
#include "../include/broadcast_manager.h"
#include <sstream>
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
// REGISTER
// ==================================================

// message client gửi: REGISTER|username|password|email|role
bool handleRegister(int clientSock, DbManager *db, const std::vector<std::string>& parts) {
    if (parts.size() != 5 || parts[0] != "REGISTER") {
        sendLine(clientSock, "REGISTER_FAIL|reason=bad_format");
        return false;
    }

    std::string username = escapeSql(parts[1]);
    std::string password = escapeSql(parts[2]);
    std::string email = escapeSql(parts[3]);
    std::string role = escapeSql(parts[4]);

    // Validate role
    if (role != "teacher" && role != "student" && role != "admin") {
        sendLine(clientSock, "REGISTER_FAIL|reason=invalid_role");
        return false;
    }

    // Check if username already exists
    std::string checkQ = "SELECT username FROM Users WHERE username = '" + username + "';";
    sql::ResultSet *checkRes = nullptr;
    try {
        checkRes = db->executeQuery(checkQ);
        if (checkRes && checkRes->next()) {
            delete checkRes;
            sendLine(clientSock, "REGISTER_FAIL|reason=username_exists");
            return false;
        }
        delete checkRes;
    } catch (sql::SQLException &e) {
        std::cerr << "[REGISTER] SQL error checking username: " << e.what() << std::endl;
        sendLine(clientSock, "REGISTER_FAIL|reason=sql_error");
        return false;
    }

    // Insert new user
    std::string insertQ = 
        "INSERT INTO Users (username, password, email, role) VALUES ('" +
        username + "', '" + password + "', '" + email + "', '" + role + "');";

    std::cout << "[REGISTER] SQL Query: " << insertQ << std::endl;

    try {
        db->executeUpdate(insertQ);
        sendLine(clientSock, "REGISTER_OK");
        std::cout << "[REGISTER] New user registered: " << username << " as " << role << std::endl;
        return true;
    } catch (sql::SQLException &e) {
        std::cerr << "[REGISTER] SQL error inserting user: " << e.what() << std::endl;
        sendLine(clientSock, "REGISTER_FAIL|reason=sql_error");
        return false;
    }
}

// ==================================================
// STUDENT EXAM HANDLERS
// ==================================================

// Helper: Send a question to client
// Format: QUESTION|qId|text|A|B|C|D
void sendQuestion(int sock, int questionId, DbManager* db) {
    try {
        // Get question text
        std::string qSql = 
            "SELECT question_text FROM Questions "
            "WHERE question_id = " + std::to_string(questionId) + ";";
        
        sql::ResultSet* qRes = db->executeQuery(qSql);
        if (!qRes || !qRes->next()) {
            delete qRes;
            sendLine(sock, "QUESTION_FAIL|reason=question_not_found");
            return;
        }
        std::string questionText = qRes->getString("question_text");
        delete qRes;
        
        // Get answers (A, B, C, D)
        std::string aSql = 
            "SELECT answer_text FROM Answers "
            "WHERE question_id = " + std::to_string(questionId) + 
            " ORDER BY answer_id LIMIT 4;";
        
        sql::ResultSet* aRes = db->executeQuery(aSql);
        if (!aRes) {
            sendLine(sock, "QUESTION_FAIL|reason=answers_not_found");
            return;
        }
        
        std::vector<std::string> answers;
        while (aRes->next() && answers.size() < 4) {
            answers.push_back(aRes->getString("answer_text"));
        }
        delete aRes;
        
        // Pad with empty strings if less than 4 answers
        while (answers.size() < 4) {
            answers.push_back("");
        }
        
        // Send question: QUESTION|qId|text|A|B|C|D
        std::stringstream ss;
        ss << "QUESTION|" << questionId << "|" << questionText << "|"
           << answers[0] << "|" << answers[1] << "|" 
           << answers[2] << "|" << answers[3];
        
        sendLine(sock, ss.str());
        std::cout << "[SEND_QUESTION] Sent question " << questionId << " to client " << sock << std::endl;
        
    } catch (sql::SQLException& e) {
        std::cerr << "[SEND_QUESTION] SQL error: " << e.what() << std::endl;
        sendLine(sock, "QUESTION_FAIL|reason=sql_error");
    }
}

// JOIN_ROOM|quizId
void handleJoinRoom(const std::vector<std::string>& parts, int sock, 
                   DbManager* db, ClientManager& clientMgr) {
    if (parts.size() != 2) {
        sendLine(sock, "JOIN_FAIL|reason=bad_format");
        return;
    }
    
    ClientInfo* clientInfo = clientMgr.getClient(sock);
    if (!clientInfo || clientInfo->role != "student") {
        sendLine(sock, "JOIN_FAIL|reason=permission_denied");
        return;
    }
    
    int quizId;
    try {
        quizId = std::stoi(parts[1]);
    } catch (...) {
        sendLine(sock, "JOIN_FAIL|reason=bad_number");
        return;
    }
    
    try {
        // Check if quiz exists and has questions
        std::string checkQuizSql = 
            "SELECT q.quiz_id, q.time_limit, q.exam_type, "
            "       q.exam_start_time, q.exam_end_time, "
            "       COUNT(qu.question_id) as question_count "
            "FROM Quizzes q "
            "LEFT JOIN Questions qu ON q.quiz_id = qu.quiz_id "
            "WHERE q.quiz_id = " + std::to_string(quizId) + 
            " GROUP BY q.quiz_id;";
        
        sql::ResultSet* checkRes = db->executeQuery(checkQuizSql);
        if (!checkRes || !checkRes->next()) {
            delete checkRes;
            sendLine(sock, "JOIN_FAIL|reason=quiz_not_found");
            return;
        }
        
        int questionCount = checkRes->getInt("question_count");
        int timeLimit = checkRes->getInt("time_limit");
        std::string examType = checkRes->getString("exam_type");
        delete checkRes;
        
        if (questionCount == 0) {
            sendLine(sock, "JOIN_FAIL|reason=quiz_has_no_questions");
            return;
        }
        
        // Check scheduled exam time window
        if (examType == "scheduled") {
            std::string timeCheckSql = 
                "SELECT q.exam_start_time, q.exam_end_time, "
                "       NOW() as current_time, "
                "       (NOW() >= q.exam_start_time AND NOW() <= q.exam_end_time) as in_time_window "
                "FROM Quizzes q "
                "WHERE q.quiz_id = " + std::to_string(quizId) + ";";
            
            sql::ResultSet* timeRes = db->executeQuery(timeCheckSql);
            if (timeRes && timeRes->next()) {
                bool inTimeWindow = timeRes->getInt("in_time_window") == 1;
                std::string startTime = timeRes->getString("exam_start_time");
                std::string endTime = timeRes->getString("exam_end_time");
                std::string currentTime = timeRes->getString("current_time");
                delete timeRes;
                
                if (!inTimeWindow) {
                    std::stringstream ss;
                    ss << "JOIN_FAIL|reason=exam_not_available|"
                       << "start_time=" << startTime << "|"
                       << "end_time=" << endTime << "|"
                       << "current_time=" << currentTime;
                    sendLine(sock, ss.str());
                    return;
                }
            } else {
                delete timeRes;
                sendLine(sock, "JOIN_FAIL|reason=time_check_failed");
                return;
            }
        }
        
        // Start exam
        int examId = startExam(clientInfo->userId, quizId, db);
        if (examId <= 0) {
            sendLine(sock, "JOIN_FAIL|reason=failed_to_start_exam");
            return;
        }
        
        // Get questions for this quiz
        std::vector<int> questionIds = getQuestionsForQuiz(quizId, db);
        if (questionIds.empty()) {
            sendLine(sock, "JOIN_FAIL|reason=no_questions_available");
            return;
        }
        
        // Update client info with exam state
        clientInfo->currentExamId = examId;
        clientInfo->currentQuestionIndex = 0;
        clientInfo->questionIds = questionIds;
        
        // Send TEST_STARTED with time limit (in minutes)
        int timeLimitMinutes = timeLimit / 60;
        sendLine(sock, "TEST_STARTED|" + std::to_string(timeLimitMinutes));
        
        // Send first question
        sendQuestion(sock, questionIds[0], db);
        
        std::cout << "[JOIN_ROOM] Student " << clientInfo->userId 
                  << " joined quiz " << quizId << ", examId=" << examId << std::endl;
        
    } catch (sql::SQLException& e) {
        std::cerr << "[JOIN_ROOM] SQL error: " << e.what() << std::endl;
        sendLine(sock, "JOIN_FAIL|reason=sql_error");
    }
}

// ANSWER|qId|choice
void handleAnswer(const std::vector<std::string>& parts, int sock,
                 DbManager* db, ClientManager& clientMgr) {
    if (parts.size() != 3) {
        sendLine(sock, "ANSWER_FAIL|reason=bad_format");
        return;
    }
    
    ClientInfo* clientInfo = clientMgr.getClient(sock);
    if (!clientInfo || clientInfo->role != "student") {
        sendLine(sock, "ANSWER_FAIL|reason=permission_denied");
        return;
    }
    
    if (clientInfo->currentExamId == 0) {
        sendLine(sock, "ANSWER_FAIL|reason=no_active_exam");
        return;
    }
    
    int questionId;
    try {
        questionId = std::stoi(parts[1]);
    } catch (...) {
        sendLine(sock, "ANSWER_FAIL|reason=bad_question_id");
        return;
    }
    
    std::string choice = parts[2]; // A, B, C, or D
    
    try {
        // Get answer text based on choice (A=1, B=2, C=3, D=4)
        int answerIndex = 0;
        if (choice == "A" || choice == "a") answerIndex = 1;
        else if (choice == "B" || choice == "b") answerIndex = 2;
        else if (choice == "C" || choice == "c") answerIndex = 3;
        else if (choice == "D" || choice == "d") answerIndex = 4;
        else {
            sendLine(sock, "ANSWER_FAIL|reason=invalid_choice");
            return;
        }
        
        // Get answer_id for this question and index
        std::string getAnswerSql = 
            "SELECT answer_id, answer_text FROM Answers "
            "WHERE question_id = " + std::to_string(questionId) + 
            " ORDER BY answer_id LIMIT 4 OFFSET " + std::to_string(answerIndex - 1) + ";";
        
        sql::ResultSet* answerRes = db->executeQuery(getAnswerSql);
        if (!answerRes || !answerRes->next()) {
            delete answerRes;
            sendLine(sock, "ANSWER_FAIL|reason=answer_not_found");
            return;
        }
        
        int answerId = answerRes->getInt("answer_id");
        std::string answerText = answerRes->getString("answer_text");
        delete answerRes;
        
        // Save answer to Exam_Answers (delete old answer if exists, then insert new)
        std::string deleteSql = 
            "DELETE FROM Exam_Answers "
            "WHERE exam_id = " + std::to_string(clientInfo->currentExamId) + 
            " AND question_id = " + std::to_string(questionId) + ";";
        
        db->executeUpdate(deleteSql);
        
        std::string insertSql = 
            "INSERT INTO Exam_Answers (exam_id, question_id, answer_id, chosen_answer) "
            "VALUES (" + std::to_string(clientInfo->currentExamId) + ", " +
            std::to_string(questionId) + ", " + std::to_string(answerId) + ", '" +
            escapeSql(answerText) + "');";
        
        db->executeUpdate(insertSql);
        
        // Move to next question
        clientInfo->currentQuestionIndex++;
        
        // Check if there are more questions
        if (clientInfo->currentQuestionIndex < clientInfo->questionIds.size()) {
            // Send next question
            int nextQuestionId = clientInfo->questionIds[clientInfo->currentQuestionIndex];
            sendQuestion(sock, nextQuestionId, db);
        } else {
            // All questions answered, submit exam and send results
            int examId = clientInfo->currentExamId; // Save before reset
            submitExam(examId, db);
            
            // Get score
            std::string getScoreSql = 
                "SELECT score FROM Exams WHERE exam_id = " + 
                std::to_string(examId) + ";";
            
            sql::ResultSet* scoreRes = db->executeQuery(getScoreSql);
            double score = 0.0;
            int correctCount = 0;
            if (scoreRes && scoreRes->next()) {
                score = scoreRes->getDouble("score");
                // Calculate correct count from score (score = (correct/total) * 10)
                if (score > 0 && clientInfo->questionIds.size() > 0) {
                    correctCount = int((score / 10.0) * clientInfo->questionIds.size() + 0.5);
                }
            }
            delete scoreRes;
            
            // Reset exam state
            clientInfo->currentExamId = 0;
            clientInfo->currentQuestionIndex = 0;
            clientInfo->questionIds.clear();
            
            // Send END_EXAM with score
            std::stringstream ss;
            ss << "END_EXAM|" << score << "|" << correctCount;
            sendLine(sock, ss.str());
            
            std::cout << "[ANSWER] Exam " << examId 
                      << " completed, score = " << score << std::endl;
        }
        
    } catch (sql::SQLException& e) {
        std::cerr << "[ANSWER] SQL error: " << e.what() << std::endl;
        sendLine(sock, "ANSWER_FAIL|reason=sql_error");
    }
}

// LIST_MY_HISTORY
void handleListMyHistory(int sock, DbManager* db, ClientManager& clientMgr) {
    ClientInfo* clientInfo = clientMgr.getClient(sock);
    if (!clientInfo || clientInfo->role != "student") {
        sendLine(sock, "HISTORY_FAIL|reason=permission_denied");
        return;
    }
    
    try {
        std::string sql = 
            "SELECT q.title, e.score, e.start_time, e.end_time "
            "FROM Exams e "
            "JOIN Quizzes q ON e.quiz_id = q.quiz_id "
            "WHERE e.user_id = " + std::to_string(clientInfo->userId) + 
            " AND e.status = 'submitted' "
            "ORDER BY e.end_time DESC;";
        
        sql::ResultSet* res = db->executeQuery(sql);
        if (!res) {
            sendLine(sock, "HISTORY_FAIL|reason=db_error");
            return;
        }
        
        std::stringstream ss;
        ss << "HISTORY|";
        bool first = true;
        while (res->next()) {
            if (!first) ss << ";";
            first = false;
            std::string title = res->getString("title");
            double score = res->getDouble("score");
            ss << title << ": " << score << "đ";
        }
        delete res;
        
        if (first) {
            // No history
            sendLine(sock, "HISTORY|Chưa có lịch sử thi");
        } else {
            sendLine(sock, ss.str());
        }
        
    } catch (sql::SQLException& e) {
        std::cerr << "[LIST_MY_HISTORY] SQL error: " << e.what() << std::endl;
        sendLine(sock, "HISTORY_FAIL|reason=sql_error");
    }
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
        
    } else if (cmd == "JOIN_ROOM") {
        handleJoinRoom(parts, clientSock, db, clientMgr);
        
    } else if (cmd == "ANSWER") {
        handleAnswer(parts, clientSock, db, clientMgr);
        
    } else if (cmd == "LIST_MY_HISTORY") {
        handleListMyHistory(clientSock, db, clientMgr);
        
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
                
                // Handle login/register for unauthenticated clients
                if (!info || !info->authenticated) {
                    std::string line = recvLine(clientSock);
                    if (line.empty()) {
                        std::cout << "[NET] Client " << clientSock << " disconnected\n";
                        close(clientSock);
                        FD_CLR(clientSock, &master_set);
                        clientMgr.removeClient(clientSock);
                        continue;
                    }

                    auto parts = split(line, '|');
                    if (parts.empty()) {
                        close(clientSock);
                        FD_CLR(clientSock, &master_set);
                        clientMgr.removeClient(clientSock);
                        continue;
                    }

                    if (parts[0] == "REGISTER") {
                        // Handle registration
                        if (handleRegister(clientSock, db, parts)) {
                            std::cout << "[NET] Client " << clientSock << " registered successfully\n";
                        }
                        // Close connection after register (client must login separately)
                        close(clientSock);
                        FD_CLR(clientSock, &master_set);
                        clientMgr.removeClient(clientSock);
                        continue;
                    } else if (parts[0] == "LOGIN") {
                        // Re-construct the login message for handleLogin
                        std::string loginMsg = line;
                        // handleLogin expects to read from socket, so we need to simulate that
                        // Since we already read the line, we'll pass it differently
                        
                        // Parse login info directly
                        if (parts.size() != 3) {
                            sendLine(clientSock, "LOGIN_FAIL|reason=bad_format");
                            close(clientSock);
                            FD_CLR(clientSock, &master_set);
                            clientMgr.removeClient(clientSock);
                            continue;
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
                            if (res && res->next()) {
                                std::string role = res->getString("role");
                                int userId = res->getInt("user_id");
                                
                                // Create session
                                SessionManager& sessionMgr = SessionManager::getInstance();
                                std::string sessionId = sessionMgr.createSession(userId, username, role);
                                
                                sendLine(clientSock, "LOGIN_OK|sessionId=" + sessionId + "|role=" + role);
                                std::cout << "[LOGIN] user " << username << " logged in as " << role << std::endl;
                                
                                // Get user info from session
                                Session* session = sessionMgr.getSession(sessionId);
                                if (session) {
                                    clientMgr.setClientInfo(clientSock, sessionId, role, 
                                                           session->username, session->userId);
                                    std::cout << "[NET] Client " << clientSock 
                                             << " authenticated as " << role << std::endl;
                                }
                                delete res;
                            } else {
                                sendLine(clientSock, "LOGIN_FAIL|reason=wrong_credentials");
                                delete res;
                                close(clientSock);
                                FD_CLR(clientSock, &master_set);
                                clientMgr.removeClient(clientSock);
                                continue;
                            }
                        } catch (sql::SQLException &e) {
                            std::cerr << "[LOGIN] SQL error: " << e.what() << std::endl;
                            sendLine(clientSock, "LOGIN_FAIL|reason=sql_error");
                            close(clientSock);
                            FD_CLR(clientSock, &master_set);
                            clientMgr.removeClient(clientSock);
                            continue;
                        }
                    } else {
                        // Unknown command before authentication
                        sendLine(clientSock, "ERROR|Please LOGIN or REGISTER first");
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

