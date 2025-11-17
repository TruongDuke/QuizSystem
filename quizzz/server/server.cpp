#include <iostream>
#include <string>
#include <vector>
#include <sstream>
 
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
// LOGIN
// ==================================================
 
// message client gửi: LOGIN|username|password
bool handleLogin(int clientSock, DbManager *db, string &outRole) {
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
 
    string q =
        "SELECT * FROM Users "
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
        string sessionId = "S123"; // demo
 
        sendLine(clientSock, "LOGIN_OK|sessionId=" + sessionId + "|role=" + role);
        cout << "[LOGIN] user logged in as " << role << endl;
        ok = true;
    } else {
        sendLine(clientSock, "LOGIN_FAIL|reason=wrong_credentials");
    }
 
    delete res; // tạm chấp nhận leak connection bên trong
    return ok;
}
 
// ==================================================
// QUIZZES
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
        // Xoá theo thứ tự tránh lỗi FK
        // 1) đáp án của các câu hỏi thuộc quiz
        string q1 =
            "DELETE FROM Answers WHERE question_id IN "
            "(SELECT question_id FROM Questions WHERE quiz_id=" +
            to_string(quizId) + ");";
        db->executeUpdate(q1);
 
        // 2) exam_answers của các exam thuộc quiz
        string q2 =
            "DELETE FROM Exam_Answers WHERE exam_id IN "
            "(SELECT exam_id FROM Exams WHERE quiz_id=" +
            to_string(quizId) + ");";
        db->executeUpdate(q2);
 
        // 3) exams thuộc quiz
        string q3 =
            "DELETE FROM Exams WHERE quiz_id=" +
            to_string(quizId) + ";";
        db->executeUpdate(q3);
 
        // 4) questions
        string q4 =
            "DELETE FROM Questions WHERE quiz_id=" +
            to_string(quizId) + ";";
        db->executeUpdate(q4);
 
        // 5) quiz
        string q5 =
            "DELETE FROM Quizzes WHERE quiz_id=" +
            to_string(quizId) + ";";
        db->executeUpdate(q5);
 
        sendLine(sock, "DELETE_QUIZ_OK");
    } catch (sql::SQLException &e) {
        cerr << "[DELETE_QUIZ] SQL error: " << e.what() << endl;
        sendLine(sock, "DELETE_QUIZ_FAIL|reason=sql_error");
    }
}
 
// ==================================================
// QUESTIONS
// ==================================================
 
// LIST_QUESTIONS|quizId
void handleListQuestions(const vector<string> &parts, int sock, DbManager *db) {
    if (parts.size() != 2) {
        sendLine(sock, "LIST_QUESTIONS_FAIL|reason=bad_format");
        return;
    }
    int quizId;
    try {
        quizId = stoi(parts[1]);
    } catch (...) {
        sendLine(sock, "LIST_QUESTIONS_FAIL|reason=bad_number");
        return;
    }
 
    string q =
        "SELECT question_id, question_text "
        "FROM Questions WHERE quiz_id=" + to_string(quizId) + ";";
 
    try {
        sql::ResultSet *res = db->executeQuery(q);
        if (!res) {
            sendLine(sock, "LIST_QUESTIONS_FAIL|reason=db_error");
            return;
        }
 
        stringstream ss;
        ss << "QUESTIONS|" << quizId << "|";
        bool first = true;
        while (res->next()) {
            if (!first) ss << ";";
            first = false;
            int qid = res->getInt("question_id");
            string text = res->getString("question_text");
            ss << qid << ":" << text;
        }
        delete res;
        sendLine(sock, ss.str());
    } catch (sql::SQLException &e) {
        cerr << "[LIST_QUESTIONS] SQL error: " << e.what() << endl;
        sendLine(sock, "LIST_QUESTIONS_FAIL|reason=sql_error");
    }
}
 
// ADD_QUESTION|quizId|content|opt1|opt2|opt3|opt4|correctIndex
void handleAddQuestion(const vector<string> &parts, int sock, DbManager *db) {
    if (parts.size() != 8) {
        sendLine(sock, "ADD_QUESTION_FAIL|reason=bad_format");
        return;
    }
 
    int quizId, correct;
    try {
        quizId  = stoi(parts[1]);
        correct = stoi(parts[7]); // 1..4
    } catch (...) {
        sendLine(sock, "ADD_QUESTION_FAIL|reason=bad_number");
        return;
    }
 
    string content = escapeSql(parts[2]);
    string o1 = escapeSql(parts[3]);
    string o2 = escapeSql(parts[4]);
    string o3 = escapeSql(parts[5]);
    string o4 = escapeSql(parts[6]);
 
    try {
        // 1) Questions
        string qInsert =
            "INSERT INTO Questions (quiz_id, question_text, difficulty, topic) "
            "VALUES (" + to_string(quizId) + ", '" + content +
            "', 'easy', 'general');";
 
        int questionId = db->executeInsertAndGetId(qInsert);
        if (questionId == 0) {
            sendLine(sock, "ADD_QUESTION_FAIL|reason=sql_error");
            return;
        }
 
        // 2) Answers
        auto insertAns = [&](const string &txt, int index) {
            string a =
                "INSERT INTO Answers (question_id, answer_text, is_correct) "
                "VALUES (" + to_string(questionId) + ", '" +
                escapeSql(txt) + "', " + (index == correct ? "1" : "0") + ");";
            db->executeUpdate(a);
        };
 
        insertAns(o1, 1);
        insertAns(o2, 2);
        insertAns(o3, 3);
        insertAns(o4, 4);
 
        sendLine(sock, "ADD_QUESTION_OK");
        cout << "[ADD_QUESTION] question added for quiz " << quizId
             << ", qid=" << questionId << endl;
 
    } catch (sql::SQLException &e) {
        cerr << "[ADD_QUESTION] SQL error: " << e.what() << endl;
        sendLine(sock, "ADD_QUESTION_FAIL|reason=sql_error");
    }
}
 
// EDIT_QUESTION|qId|content|opt1|opt2|opt3|opt4|correctIndex
void handleEditQuestion(const vector<string> &parts, int sock, DbManager *db) {
    if (parts.size() != 8) {
        sendLine(sock, "EDIT_QUESTION_FAIL|reason=bad_format");
        return;
    }
 
    int qid, correct;
    try {
        qid     = stoi(parts[1]);
        correct = stoi(parts[7]);
    } catch (...) {
        sendLine(sock, "EDIT_QUESTION_FAIL|reason=bad_number");
        return;
    }
 
    string content = escapeSql(parts[2]);
    string o1 = escapeSql(parts[3]);
    string o2 = escapeSql(parts[4]);
    string o3 = escapeSql(parts[5]);
    string o4 = escapeSql(parts[6]);
 
    try {
        // update question text
        string q =
            "UPDATE Questions SET question_text='" + content +
            "' WHERE question_id=" + to_string(qid) + ";";
        db->executeUpdate(q);
 
        // xoá toàn bộ answers cũ
        string d =
            "DELETE FROM Answers WHERE question_id=" +
            to_string(qid) + ";";
        db->executeUpdate(d);
 
        // insert lại 4 đáp án
        auto insertAns = [&](const string &txt, int index) {
            string a =
                "INSERT INTO Answers (question_id, answer_text, is_correct) "
                "VALUES (" + to_string(qid) + ", '" +
                escapeSql(txt) + "', " + (index == correct ? "1" : "0") + ");";
            db->executeUpdate(a);
        };
 
        insertAns(o1, 1);
        insertAns(o2, 2);
        insertAns(o3, 3);
        insertAns(o4, 4);
 
        sendLine(sock, "EDIT_QUESTION_OK");
    } catch (sql::SQLException &e) {
        cerr << "[EDIT_QUESTION] SQL error: " << e.what() << endl;
        sendLine(sock, "EDIT_QUESTION_FAIL|reason=sql_error");
    }
}
 
// DELETE_QUESTION|qId
void handleDeleteQuestion(const vector<string> &parts, int sock, DbManager *db) {
    if (parts.size() != 2) {
        sendLine(sock, "DELETE_QUESTION_FAIL|reason=bad_format");
        return;
    }
    int qid;
    try {
        qid = stoi(parts[1]);
    } catch (...) {
        sendLine(sock, "DELETE_QUESTION_FAIL|reason=bad_number");
        return;
    }
 
    try {
        string dAns =
            "DELETE FROM Answers WHERE question_id=" +
            to_string(qid) + ";";
        db->executeUpdate(dAns);
 
        string dQ =
            "DELETE FROM Questions WHERE question_id=" +
            to_string(qid) + ";";
        db->executeUpdate(dQ);
 
        sendLine(sock, "DELETE_QUESTION_OK");
    } catch (sql::SQLException &e) {
        cerr << "[DELETE_QUESTION] SQL error: " << e.what() << endl;
        sendLine(sock, "DELETE_QUESTION_FAIL|reason=sql_error");
    }
}
 
// ==================================================
// EXAMS
// ==================================================
 
// LIST_EXAMS
void handleListExams(int sock, DbManager *db) {
    string q =
        "SELECT exam_id, quiz_id, user_id, score, status "
        "FROM Exams;";
 
    try {
        sql::ResultSet *res = db->executeQuery(q);
        if (!res) {
            sendLine(sock, "LIST_EXAMS_FAIL|reason=db_error");
            return;
        }
 
        stringstream ss;
        ss << "EXAMS|";
        bool first = true;
        while (res->next()) {
            if (!first) ss << ";";
            first = false;
            int eid = res->getInt("exam_id");
            int qid = res->getInt("quiz_id");
            int uid = res->getInt("user_id");
            double score = res->getDouble("score");
            string status = res->getString("status");
            ss << eid << "(quiz=" << qid
               << ",user=" << uid
               << ",score=" << score
               << ",status=" << status << ")";
        }
        delete res;
        sendLine(sock, ss.str());
    } catch (sql::SQLException &e) {
        cerr << "[LIST_EXAMS] SQL error: " << e.what() << endl;
        sendLine(sock, "LIST_EXAMS_FAIL|reason=sql_error");
    }
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
 
    int clientSock = accept(serverSock, nullptr, nullptr);
    if (clientSock < 0) {
        cerr << "[NET] Accept failed\n";
        return 1;
    }
 
    string role;
    if (!handleLogin(clientSock, db, role)) {
        cerr << "[MAIN] login failed, closing\n";
        close(clientSock);
        close(serverSock);
        delete db;
        return 0;
    }
 
    // Vòng lặp command
    while (true) {
        string line = recvLine(clientSock);
        if (line.empty()) {
            cout << "[MAIN] client disconnected\n";
            break;
        }
 
        auto parts = split(line, '|');
        if (parts.empty()) continue;
 
        string cmd = parts[0];
 
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
 