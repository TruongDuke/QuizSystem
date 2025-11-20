#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <map> // [NEW] Thêm map để quản lý phiên làm bài

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "db_manager.h"

using namespace std;

// ==================================================
// GLOBALS: Session Management (Lưu tạm trên RAM)
// ==================================================

struct StudentSession {
    int examId;              // ID trong bảng Exams
    int currentQuestionIdx;  // Đang làm câu thứ mấy (0, 1, 2...)
    float score;             // Điểm hiện tại
    vector<int> questionIds; // Danh sách ID câu hỏi của đề thi này
};

// Map: Socket -> Session
map<int, StudentSession> sessions;

// ==================================================
// Helpers: sendLine / recvLine / split / escapeSql
// ==================================================

string recvLine(int sock) {
    string line;
    char c;
    while (true) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) {
            return "";
        }
        if (c == '\n') break;
        line.push_back(c);
    }
    return line;
}

void sendLine(int sock, const string &msg) {
    string data = msg + "\n";
    int n = send(sock, data.c_str(), data.size(), 0);
    if (n < 0) {
        cerr << "[NET] Error sending data to client\n";
    }
}

vector<string> split(const string &s, char delim) {
    vector<string> out;
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) out.push_back(item);
    return out;
}

// escape đơn giản: thay ' thành '' để tránh lỗi SQL
string escapeSql(const string &s) {
    string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\'') out += "''";
        else out.push_back(c);
    }
    return out;
}

// ==================================================
// LOGIN (Sửa đổi để lấy user_id)
// ==================================================

// message client gửi: LOGIN|username|password
// [MODIFIED] Thêm tham số outUserId
bool handleLogin(int clientSock, DbManager *db, string &outRole, int &outUserId) {
    string line = recvLine(clientSock);
    if (line.empty()) {
        cerr << "[LOGIN] client disconnected before login\n";
        return false;
    }

    auto parts = split(line, '|');
    if (parts.size() != 3 || parts[0] != "LOGIN") {
        sendLine(clientSock, "LOGIN_FAIL|reason=bad_format");
        return false;
    }

    string username = escapeSql(parts[1]);
    string password = escapeSql(parts[2]);

    // [MODIFIED] Query lấy thêm user_id
    string q =
        "SELECT user_id, role FROM Users "
        "WHERE username = '" + username +
        "' AND password = '" + password + "';";

    sql::ResultSet *res = nullptr;
    try {
        res = db->executeQuery(q);
    } catch (sql::SQLException &e) {
        cerr << "[LOGIN] SQL error: " << e.what() << endl;
        sendLine(clientSock, "LOGIN_FAIL|reason=sql_error");
        return false;
    }

    bool ok = false;
    if (res && res->next()) {
        string role = res->getString("role");
        outRole = role;
        outUserId = res->getInt("user_id"); // [NEW] Lấy user_id
        
        string sessionId = "S123"; // demo

        sendLine(clientSock, "LOGIN_OK|sessionId=" + sessionId + "|role=" + role);
        cout << "[LOGIN] user " << username << " (ID:" << outUserId << ") logged in as " << role << endl;
        ok = true;
    } else {
        sendLine(clientSock, "LOGIN_FAIL|reason=wrong_credentials");
    }

    delete res; 
    return ok;
}

// ==================================================
// QUIZZES (Teacher Logic - Giữ nguyên)
// ==================================================

// LIST_QUIZZES
void handleListQuizzes(int sock, DbManager *db) {
    string q =
        "SELECT quiz_id, title, question_count, time_limit, status "
        "FROM Quizzes;";

    try {
        sql::ResultSet *res = db->executeQuery(q);
        if (!res) {
            sendLine(sock, "LIST_QUIZZES_FAIL|reason=db_error");
            return;
        }

        // format đơn giản: QUIZZES|id:title(count,time,status);id2:...
        stringstream ss;
        ss << "QUIZZES|";
        bool first = true;
        while (res->next()) {
            if (!first) ss << ";";
            first = false;
            int id = res->getInt("quiz_id");
            string title = res->getString("title");
            int cnt = res->getInt("question_count");
            int t = res->getInt("time_limit");
            string status = res->getString("status");

            ss << id << ":" << title << "("
               << cnt << "Q," << t << "s," << status << ")";
        }
        delete res;

        sendLine(sock, ss.str());
    } catch (sql::SQLException &e) {
        cerr << "[LIST_QUIZZES] SQL error: " << e.what() << endl;
        sendLine(sock, "LIST_QUIZZES_FAIL|reason=sql_error");
    }
}

// ADD_QUIZ|title|desc|count|time
void handleAddQuiz(const vector<string> &parts, int sock, DbManager *db) {
    if (parts.size() != 5) {
        sendLine(sock, "ADD_QUIZ_FAIL|reason=bad_format");
        return;
    }

    string title = escapeSql(parts[1]);
    string desc  = escapeSql(parts[2]);

    int count = 0, timeLimit = 0;
    try {
        count = stoi(parts[3]);
        timeLimit = stoi(parts[4]);
    } catch (...) {
        sendLine(sock, "ADD_QUIZ_FAIL|reason=bad_number");
        return;
    }

    string insertSql =
        "INSERT INTO Quizzes (title, description, question_count, time_limit, "
        "status, creator_id) VALUES ('" +
        title + "', '" + desc + "', " +
        to_string(count) + ", " + to_string(timeLimit) +
        ", 'not_started', 2);";
 
    cout << "[ADD_QUIZ] SQL: " << insertSql << endl;
    int newId = db->executeInsertAndGetId(insertSql);
    if (newId == 0) {
        sendLine(sock, "ADD_QUIZ_FAIL|reason=sql_error");
        return;
    }
 
    sendLine(sock, "ADD_QUIZ_OK|quizId=" + to_string(newId));
    cout << "[ADD_QUIZ] quiz added, id=" << newId << endl;
}

// EDIT_QUIZ|quizId|title|desc|count|time
void handleEditQuiz(const vector<string> &parts, int sock, DbManager *db) {
    if (parts.size() != 6) {
        sendLine(sock, "EDIT_QUIZ_FAIL|reason=bad_format");
        return;
    }
 
    int quizId, count, timeLimit;
    try {
        quizId = stoi(parts[1]);
        count  = stoi(parts[4]);
        timeLimit = stoi(parts[5]);
    } catch (...) {
        sendLine(sock, "EDIT_QUIZ_FAIL|reason=bad_number");
        return;
    }
 
    string title = escapeSql(parts[2]);
    string desc  = escapeSql(parts[3]);
 
    string q =
        "UPDATE Quizzes SET "
        "title='" + title + "', "
        "description='" + desc + "', "
        "question_count=" + to_string(count) + ", "
        "time_limit=" + to_string(timeLimit) +
        " WHERE quiz_id=" + to_string(quizId) + ";";
 
    try {
        db->executeUpdate(q);
        sendLine(sock, "EDIT_QUIZ_OK");
    } catch (...) {
        sendLine(sock, "EDIT_QUIZ_FAIL|reason=sql_error");
    }
}

// DELETE_QUIZ|quizId
void handleDeleteQuiz(const vector<string> &parts, int sock, DbManager *db) {
    if (parts.size() != 2) {
        sendLine(sock, "DELETE_QUIZ_FAIL|reason=bad_format");
        return;
    }
 
    int quizId;
    try {
        quizId = stoi(parts[1]);
    } catch (...) {
        sendLine(sock, "DELETE_QUIZ_FAIL|reason=bad_number");
        return;
    }
 
    try {
        string q1 = "DELETE FROM Answers WHERE question_id IN (SELECT question_id FROM Questions WHERE quiz_id=" + to_string(quizId) + ");";
        db->executeUpdate(q1);
        string q2 = "DELETE FROM Exam_Answers WHERE exam_id IN (SELECT exam_id FROM Exams WHERE quiz_id=" + to_string(quizId) + ");";
        db->executeUpdate(q2);
        string q3 = "DELETE FROM Exams WHERE quiz_id=" + to_string(quizId) + ";";
        db->executeUpdate(q3);
        string q4 = "DELETE FROM Questions WHERE quiz_id=" + to_string(quizId) + ";";
        db->executeUpdate(q4);
        string q5 = "DELETE FROM Quizzes WHERE quiz_id=" + to_string(quizId) + ";";
        db->executeUpdate(q5);
 
        sendLine(sock, "DELETE_QUIZ_OK");
    } catch (sql::SQLException &e) {
        cerr << "[DELETE_QUIZ] SQL error: " << e.what() << endl;
        sendLine(sock, "DELETE_QUIZ_FAIL|reason=sql_error");
    }
}

// ==================================================
// QUESTIONS (Teacher Logic - Giữ nguyên)
// ==================================================

// LIST_QUESTIONS|quizId
void handleListQuestions(const vector<string> &parts, int sock, DbManager *db) {
    if (parts.size() != 2) {
        sendLine(sock, "LIST_QUESTIONS_FAIL|reason=bad_format");
        return;
    }
    int quizId;
    try { quizId = stoi(parts[1]); } catch (...) { sendLine(sock, "ERR"); return; }
 
    string q = "SELECT question_id, question_text FROM Questions WHERE quiz_id=" + to_string(quizId) + ";";
 
    try {
        sql::ResultSet *res = db->executeQuery(q);
        stringstream ss;
        ss << "QUESTIONS|" << quizId << "|";
        bool first = true;
        while (res && res->next()) {
            if (!first) ss << ";";
            first = false;
            ss << res->getInt("question_id") << ":" << res->getString("question_text");
        }
        delete res;
        sendLine(sock, ss.str());
    } catch (...) { sendLine(sock, "ERR"); }
}

// ADD_QUESTION|quizId|content|opt1|opt2|opt3|opt4|correctIndex
void handleAddQuestion(const vector<string> &parts, int sock, DbManager *db) {
    if (parts.size() != 8) { sendLine(sock, "ERR"); return; }
 
    int quizId = stoi(parts[1]);
    int correct = stoi(parts[7]);
    string content = escapeSql(parts[2]);
 
    try {
        string qInsert = "INSERT INTO Questions (quiz_id, question_text, difficulty, topic) VALUES (" + to_string(quizId) + ", '" + content + "', 'easy', 'general');";
        int questionId = db->executeInsertAndGetId(qInsert);
 
        auto insertAns = [&](const string &txt, int index) {
            string a = "INSERT INTO Answers (question_id, answer_text, is_correct) VALUES (" + to_string(questionId) + ", '" + escapeSql(txt) + "', " + (index == correct ? "1" : "0") + ");";
            db->executeUpdate(a);
        };
        insertAns(parts[3], 1); insertAns(parts[4], 2); insertAns(parts[5], 3); insertAns(parts[6], 4);
 
        sendLine(sock, "ADD_QUESTION_OK");
    } catch (...) { sendLine(sock, "ERR"); }
}

// EDIT_QUESTION
void handleEditQuestion(const vector<string> &parts, int sock, DbManager *db) {
    if (parts.size() != 8) { sendLine(sock, "ERR"); return; }
    int qid = stoi(parts[1]);
    int correct = stoi(parts[7]);
    try {
        db->executeUpdate("UPDATE Questions SET question_text='" + escapeSql(parts[2]) + "' WHERE question_id=" + to_string(qid));
        db->executeUpdate("DELETE FROM Answers WHERE question_id=" + to_string(qid));
        
        auto insertAns = [&](const string &txt, int index) {
            string a = "INSERT INTO Answers (question_id, answer_text, is_correct) VALUES (" + to_string(qid) + ", '" + escapeSql(txt) + "', " + (index == correct ? "1" : "0") + ");";
            db->executeUpdate(a);
        };
        insertAns(parts[3], 1); insertAns(parts[4], 2); insertAns(parts[5], 3); insertAns(parts[6], 4);
        sendLine(sock, "EDIT_QUESTION_OK");
    } catch (...) { sendLine(sock, "ERR"); }
}

// DELETE_QUESTION
void handleDeleteQuestion(const vector<string> &parts, int sock, DbManager *db) {
    if (parts.size() != 2) return;
    int qid = stoi(parts[1]);
    try {
        db->executeUpdate("DELETE FROM Answers WHERE question_id=" + to_string(qid));
        db->executeUpdate("DELETE FROM Questions WHERE question_id=" + to_string(qid));
        sendLine(sock, "DELETE_QUESTION_OK");
    } catch (...) { sendLine(sock, "ERR"); }
}

// LIST_EXAMS (Teacher View)
void handleListExams(int sock, DbManager *db) {
    string q = "SELECT exam_id, quiz_id, user_id, score, status FROM Exams;";
    try {
        sql::ResultSet *res = db->executeQuery(q);
        stringstream ss; ss << "EXAMS|";
        bool first = true;
        while (res && res->next()) {
            if (!first) ss << ";"; first = false;
            ss << res->getInt("exam_id") << "(quiz=" << res->getInt("quiz_id") << ",user=" << res->getInt("user_id") << ",score=" << res->getDouble("score") << ",status=" << res->getString("status") << ")";
        }
        delete res;
        sendLine(sock, ss.str());
    } catch (...) { sendLine(sock, "ERR"); }
}

// ==================================================
// STUDENT LOGIC (NEW)
// ==================================================

// Helper: Gửi câu hỏi tiếp theo cho học sinh
void sendNextQuestion(int sock, DbManager *db) {
    if (sessions.find(sock) == sessions.end()) return;
    StudentSession &s = sessions[sock];

    // 1. Kiểm tra nếu đã hết câu hỏi
    if (s.currentQuestionIdx >= s.questionIds.size()) {
        // Cập nhật trạng thái bài thi là 'submitted'
        string endSql = "UPDATE Exams SET status='submitted', end_time=NOW(), score=" + to_string(s.score) 
                      + " WHERE exam_id=" + to_string(s.examId);
        db->executeUpdate(endSql);

        // Gửi kết quả cuối cùng
        sendLine(sock, "END_EXAM|" + to_string(s.score));
        
        // Xóa session
        sessions.erase(sock);
        return;
    }

    // 2. Lấy câu hỏi hiện tại
    int qId = s.questionIds[s.currentQuestionIdx];

    // Query nội dung câu hỏi
    string qText;
    sql::ResultSet *resQ = db->executeQuery("SELECT question_text FROM Questions WHERE question_id=" + to_string(qId));
    if (resQ && resQ->next()) qText = resQ->getString("question_text");
    delete resQ;

    // Query 4 đáp án
    vector<string> answers;
    sql::ResultSet *resA = db->executeQuery("SELECT answer_text FROM Answers WHERE question_id=" + to_string(qId) + " LIMIT 4");
    while (resA && resA->next()) answers.push_back(resA->getString("answer_text"));
    delete resA;
    
    while (answers.size() < 4) answers.push_back(""); // Padding nếu thiếu

    // Gửi: QUESTION|qId|Text|A|B|C|D
    string msg = "QUESTION|" + to_string(qId) + "|" + qText + "|" 
               + answers[0] + "|" + answers[1] + "|" + answers[2] + "|" + answers[3];
    sendLine(sock, msg);
}

// Xử lý tham gia thi: JOIN_ROOM|quizId
void handleStudentJoinExam(const vector<string> &parts, int sock, int userId, DbManager *db) {
    if (parts.size() < 2) return;
    int quizId = stoi(parts[1]);

    try {
        // 1. Tạo record trong Exams (status: in_progress)
        string sql = "INSERT INTO Exams (quiz_id, user_id, start_time, score, status) VALUES (" 
                   + to_string(quizId) + ", " + to_string(userId) + ", NOW(), 0, 'in_progress')";
        int examId = db->executeInsertAndGetId(sql);

        // 2. Lấy danh sách câu hỏi
        vector<int> qIds;
        sql::ResultSet *res = db->executeQuery("SELECT question_id FROM Questions WHERE quiz_id=" + to_string(quizId));
        while (res && res->next()) qIds.push_back(res->getInt("question_id"));
        delete res;

        if (qIds.empty()) {
            sendLine(sock, "JOIN_FAIL|Empty_Quiz");
            return;
        }

        // 3. Tạo Session
        StudentSession s;
        s.examId = examId;
        s.currentQuestionIdx = 0;
        s.score = 0;
        s.questionIds = qIds;
        sessions[sock] = s;

        // 4. Bắt đầu
        sendLine(sock, "TEST_STARTED");
        sendNextQuestion(sock, db);

    } catch (sql::SQLException &e) {
        cerr << "[JOIN] SQL Error: " << e.what() << endl;
        sendLine(sock, "JOIN_FAIL|DB_Error");
    }
}

// Xử lý trả lời: ANSWER|qId|Choice (A/B/C/D)
void handleStudentAnswer(const vector<string> &parts, int sock, DbManager *db) {
    if (sessions.find(sock) == sessions.end()) return;
    if (parts.size() < 3) return;

    StudentSession &s = sessions[sock];
    int qId = stoi(parts[1]);
    string choiceChar = parts[2]; // "A", "B", "C", "D"

    // Chuyển A->0, B->1... để lấy đúng dòng trong DB
    int choiceIdx = 0;
    if (choiceChar == "B") choiceIdx = 1;
    else if (choiceChar == "C") choiceIdx = 2;
    else if (choiceChar == "D") choiceIdx = 3;

    // Lấy ID đáp án và kiểm tra đúng sai từ DB
    // Lưu ý: Logic này giả định thứ tự insert answers là 1,2,3,4
    string sql = "SELECT answer_id, answer_text, is_correct FROM Answers WHERE question_id=" 
               + to_string(qId) + " LIMIT 1 OFFSET " + to_string(choiceIdx);
    
    try {
        sql::ResultSet *res = db->executeQuery(sql);
        if (res && res->next()) {
            int ansId = res->getInt("answer_id");
            string ansText = res->getString("answer_text");
            bool isCorrect = res->getBoolean("is_correct");

            // Lưu vào Exam_Answers
            string ins = "INSERT INTO Exam_Answers (exam_id, question_id, answer_id, chosen_answer) VALUES ("
                       + to_string(s.examId) + ", " + to_string(qId) + ", " + to_string(ansId) + ", '" + escapeSql(ansText) + "')";
            db->executeUpdate(ins);

            // Cộng điểm nếu đúng
            if (isCorrect) {
                s.score += 1.0; // Mỗi câu 1 điểm
                // Cập nhật điểm tạm thời vào Exams
                db->executeUpdate("UPDATE Exams SET score=" + to_string(s.score) + " WHERE exam_id=" + to_string(s.examId));
            }
        }
        if(res) delete res;
    } catch (...) {}

    // Chuyển sang câu tiếp theo
    s.currentQuestionIdx++;
    sendNextQuestion(sock, db);
}

// Xem lịch sử: LIST_MY_HISTORY
void handleListHistory(int sock, int userId, DbManager *db) {
    string sql = "SELECT e.start_time, q.title, e.score, e.status FROM Exams e "
                 "JOIN Quizzes q ON e.quiz_id = q.quiz_id WHERE e.user_id=" + to_string(userId);
    try {
        sql::ResultSet *res = db->executeQuery(sql);
        stringstream ss;
        ss << "HISTORY|";
        bool first = true;
        while (res && res->next()) {
            if (!first) ss << ";";
            first = false;
            ss << "[" << res->getString("start_time") << "] " << res->getString("title") 
               << " - Score: " << res->getDouble("score");
        }
        delete res;
        sendLine(sock, ss.str());
    } catch (...) { sendLine(sock, "ERR"); }
}

// ==================================================
// MAIN
// ==================================================
 
int main() {
    DbManager *db = new DbManager("127.0.0.1", "root", "123456", "quizDB");
    cout << "[DB] Connected to quizDB\n";
 
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        cerr << "[NET] Error creating socket\n";
        return 1;
    }
 
    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(9000);
    addr.sin_addr.s_addr = INADDR_ANY;
 
    if (bind(serverSock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        cerr << "[NET] Bind failed\n";
        return 1;
    }
 
    if (listen(serverSock, 5) < 0) {
        cerr << "[NET] Listen failed\n";
        return 1;
    }
 
    cout << "[NET] Server is listening on port 9000...\n";
 
    // Chấp nhận client (Single thread loop)
    while (true) {
        int clientSock = accept(serverSock, nullptr, nullptr);
        if (clientSock < 0) {
            cerr << "[NET] Accept failed\n";
            continue;
        }
        cout << "[NET] Client connected.\n";
 
        string role;
        int userId = -1; // Biến để lưu ID người dùng

        // [MODIFIED] Gọi hàm Login mới (lấy cả ID)
        if (!handleLogin(clientSock, db, role, userId)) {
            cerr << "[MAIN] Login failed or disconnected\n";
            close(clientSock);
            continue; // Đợi client tiếp theo
        }
 
        // Vòng lặp command
        while (true) {
            string line = recvLine(clientSock);
            if (line.empty()) {
                cout << "[MAIN] Client disconnected\n";
                break;
            }
 
            auto parts = split(line, '|');
            if (parts.empty()) continue;
 
            string cmd = parts[0];
 
            if (cmd == "QUIT") {
                sendLine(clientSock, "BYE");
                break;
            }

            // =========================================
            // TEACHER COMMANDS
            // =========================================
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
            }
            // =========================================
            // STUDENT COMMANDS (NEW)
            // =========================================
            else if (role == "student") {
                if (cmd == "LIST_QUIZZES") handleListQuizzes(clientSock, db); // Student cũng cần xem đề để lấy ID
                else if (cmd == "JOIN_ROOM") handleStudentJoinExam(parts, clientSock, userId, db);
                else if (cmd == "ANSWER") handleStudentAnswer(parts, clientSock, db);
                else if (cmd == "LIST_MY_HISTORY") handleListHistory(clientSock, userId, db);
                else sendLine(clientSock, "ERR|unknown_command");
            }
            else {
                sendLine(clientSock, "ERR|permission_denied");
            }
        }
 
        // Dọn dẹp session nếu có
        if (sessions.find(clientSock) != sessions.end()) {
            sessions.erase(clientSock);
        }

        close(clientSock);
        // Quay lại vòng lặp để accept người tiếp theo
    }
 
    close(serverSock);
    delete db;
    return 0;
}