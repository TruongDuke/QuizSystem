#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <limits>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace std;
 
// ==================================================

// Helpers: sendLine / recvLine / split

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
    send(sock, data.c_str(), data.size(), 0);

}
 
vector<string> split(const string &s, char delim) {
    vector<string> out;
    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) out.push_back(item);
    return out;
}
 
// Forward declaration

void teacherMenu(int sock);

void manageQuestionsMenu(int sock, int quizId);
 
// ==================================================

// Login

// ==================================================
 
string doLogin(int sock) {
    string username, password;
    cout << "Enter username: ";
    cin >> username;
    cout << "Enter password: ";
    cin >> password;
    string msg = "LOGIN|" + username + "|" + password;
    sendLine(sock, msg);
    string resp = recvLine(sock);
    if (resp.empty()) {
        cerr << "No response from server\n";
        return "";
    }
 
    cout << "Server: " << resp << endl;
    auto parts = split(resp, '|');
    if (parts.size() == 0) return "";

    if (parts[0] == "LOGIN_OK") {
        string role = "";
        for (auto &p : parts) {
            if (p.rfind("role=", 0) == 0) {
                role = p.substr(5);
            }
        }

        cout << "Logged in as: " << role << endl;
        return role;
    }
 
    cerr << "Login failed.\n";
    return "";

}
 
// ==================================================

// Teacher: Manage Questions Submenu

// ==================================================
 
void manageQuestionsMenu(int sock, int quizId) {
    while (true) {
        int choice;
        cout << "\n=========== MANAGE QUESTIONS ===========\n";
        cout << "Quiz ID: " << quizId << "\n";
        cout << "1. View questions\n";
        cout << "2. Add question\n";
        cout << "3. Edit question\n";
        cout << "4. Delete question\n";
        cout << "5. Back\n";
        cout << "========================================\n";
        cout << "Enter choice: ";
        cin >> choice;
 
        if (choice == 1) {
            sendLine(sock, "LIST_QUESTIONS|" + to_string(quizId));
            cout << "Server: " << recvLine(sock) << endl; 
        } else if (choice == 2) {
            string q, o1, o2, o3, o4;
            int correct;
            cin.ignore();
            cout << "Enter question text: ";
            getline(cin, q);
            cout << "Option 1: "; getline(cin, o1);
            cout << "Option 2: "; getline(cin, o2);
            cout << "Option 3: "; getline(cin, o3);
            cout << "Option 4: "; getline(cin, o4);
            cout << "Correct option (1-4): ";
            cin >> correct; 
            string msg = 
                "ADD_QUESTION|" + to_string(quizId) + "|" + q + "|" + 
                o1 + "|" + o2 + "|" + o3 + "|" + o4 + "|" +
                to_string(correct);
            sendLine(sock, msg);
            cout << "Server: " << recvLine(sock) << endl;
        } else if (choice == 3) {
            int qid, correct;

            string q, o1, o2, o3, o4;
 
            cout << "Enter question ID to edit: ";

            cin >> qid;

            cin.ignore();
 
            cout << "New text: "; getline(cin, q);

            cout << "New opt1: "; getline(cin, o1);

            cout << "New opt2: "; getline(cin, o2);

            cout << "New opt3: "; getline(cin, o3);

            cout << "New opt4: "; getline(cin, o4);
 
            cout << "Correct option: ";

            cin >> correct;
 
            string msg =

                "EDIT_QUESTION|" + to_string(qid) + "|" + q + "|" +

                o1 + "|" + o2 + "|" + o3 + "|" + o4 + "|" +

                to_string(correct);
 
            sendLine(sock, msg);

            cout << "Server: " << recvLine(sock) << endl;
 
        } else if (choice == 4) {

            int qid;

            cout << "Enter question ID: ";

            cin >> qid;
 
            sendLine(sock, "DELETE_QUESTION|" + to_string(qid));

            cout << "Server: " << recvLine(sock) << endl;
 
        } else if (choice == 5) {

            break;

        }

    }

}
 
// ==================================================

// Teacher Menu (FULL VERSION)

// ==================================================
 
void teacherMenu(int sock) {

    while (true) {

        int choice;
 
        cout << "\n====================================\n";

        cout << "           TEACHER MENU\n";

        cout << "====================================\n";

        cout << "1. View all quizzes\n";

        cout << "2. Add new quiz\n";

        cout << "3. Edit quiz\n";

        cout << "4. Delete quiz\n";

        cout << "5. Manage questions of a quiz\n";

        cout << "6. View exam results\n";

        cout << "7. Logout\n";

        cout << "====================================\n";

        cout << "Enter your choice: ";

        cin >> choice;
 
        if (choice == 1) {

            sendLine(sock, "LIST_QUIZZES");

            cout << "Server: " << recvLine(sock) << endl;
 
        } else if (choice == 2) {

            string title, desc;

            int count, timeLimit;
 
            cin.ignore();

            cout << "Enter quiz title: ";

            getline(cin, title);
 
            cout << "Enter description: ";

            getline(cin, desc);
 
            cout << "Enter question count: ";

            cin >> count;
 
            cout << "Enter time limit: ";

            cin >> timeLimit;
 
            string msg = 

                "ADD_QUIZ|" + title + "|" + desc + "|" +

                to_string(count) + "|" + to_string(timeLimit);
 
            sendLine(sock, msg);

            cout << "Server: " << recvLine(sock) << endl;
 
        } else if (choice == 3) {

            int quizId;

            string title, desc;

            int count, timeLimit;
 
            cout << "Enter quiz ID to edit: ";

            cin >> quizId;
 
            cin.ignore();

            cout << "New title: ";

            getline(cin, title);
 
            cout << "New description: ";

            getline(cin, desc);
 
            cout << "New question count: ";

            cin >> count;
 
            cout << "New time limit: ";

            cin >> timeLimit;
 
            string msg =

                "EDIT_QUIZ|" + to_string(quizId) + "|" +

                title + "|" + desc + "|" +

                to_string(count) + "|" +

                to_string(timeLimit);
 
            sendLine(sock, msg);

            cout << "Server: " << recvLine(sock) << endl;
 
        } else if (choice == 4) {

            int quizId;

            cout << "Enter quiz ID to delete: ";

            cin >> quizId;
 
            sendLine(sock, "DELETE_QUIZ|" + to_string(quizId));

            cout << "Server: " << recvLine(sock) << endl;
 
        } else if (choice == 5) {

            int quizId;

            cout << "Enter quiz ID to manage questions: ";

            cin >> quizId;
 
            manageQuestionsMenu(sock, quizId);
 
        } else if (choice == 6) {

            sendLine(sock, "LIST_EXAMS");

            cout << "Server: " << recvLine(sock) << endl;
 
        } else if (choice == 7) {

            sendLine(sock, "QUIT");

            cout << "Logging out...\n";

            break;
 
        } else {

            cout << "Invalid choice.\n";

        }

    }

}
 
// ==================================================

// EXAM FLOW FOR STUDENTS

// ==================================================

void enterExamRoom(int sock, string roomId) {
    cout << "\n[ROOM " << roomId << "] Joined successfully.\n";
    cout << "Waiting for teacher to start the exam...\n";

    // 1. Vòng lặp chờ bắt đầu
    while (true) {
        string msg = recvLine(sock);
        if (msg.empty()) return; // Server ngắt kết nối

        auto parts = split(msg, '|');
        if (parts.empty()) continue;

        if (parts[0] == "TEST_STARTED") {
            cout << "\n>>> EXAM STARTED! <<<\n";
            if (parts.size() > 1) cout << "Time limit: " << parts[1] << " minutes.\n";
            break; 
        } else {
            cout << "Server: " << msg << endl; // In các thông báo khác (ví dụ: chat)
        }
    }

    // 2. Vòng lặp làm bài (Nhận câu hỏi -> Trả lời)
    while (true) {
        string msg = recvLine(sock);
        if (msg.empty()) break;

        auto parts = split(msg, '|');
        if (parts.empty()) continue;

        string cmd = parts[0];

        if (cmd == "QUESTION") {
            // Format: QUESTION|qId|Text|A|B|C|D
            if (parts.size() < 7) continue;
            cout << "\n-----------------------------------";
            cout << "\nQuestion: " << parts[2] << endl;
            cout << "A. " << parts[3] << endl;
            cout << "B. " << parts[4] << endl;
            cout << "C. " << parts[5] << endl;
            cout << "D. " << parts[6] << endl;
            
            string ans;
            cout << "Your answer (A/B/C/D): ";
            cin >> ans;

            // Gửi đáp án về server: ANSWER|qId|ans
            sendLine(sock, "ANSWER|" + parts[1] + "|" + ans);

        } else if (cmd == "END_EXAM") {
            // Format: END_EXAM|score|correct_count
            cout << "\n===================================";
            cout << "\n           EXAM FINISHED           ";
            cout << "\n===================================";
            if (parts.size() >= 3) {
                cout << "\nYour Score: " << parts[1];
                cout << "\nCorrect Answers: " << parts[2];
            }
            cout << "\nPress Enter to return to menu...";
            cin.ignore();
            cin.get();
            break;
        }
    }
}

// ==================================================

// STUDENT MENU

// ==================================================

void studentMenu(int sock) {
    while (true) {
        int choice;
        cout << "\n====================================\n";
        cout << "           STUDENT MENU\n";
        cout << "====================================\n";
        cout << "1. Join Exam Room (Tham gia thi)\n";
        cout << "2. View Exam History (Lịch sử thi)\n";
        cout << "3. View Exam Results (Tra cứu điểm)\n"; // Có thể gộp với lịch sử
        cout << "4. Logout\n";
        cout << "====================================\n";
        cout << "Enter your choice: ";
        cin >> choice;

        if (choice == 1) {
            string roomId;
            cout << "Enter Room ID: ";
            cin >> roomId;

            // Gửi lệnh tham gia
            sendLine(sock, "JOIN_ROOM|" + roomId);
            string resp = recvLine(sock);
            auto parts = split(resp, '|');

            if (parts.size() > 0 && parts[0] == "JOIN_OK") {
                // Nếu vào được phòng -> Chuyển sang màn hình chờ thi
                enterExamRoom(sock, roomId);
            } else {
                cout << "Failed to join room. Server said: " << resp << endl;
            }

        } else if (choice == 2 || choice == 3) {
            // Gửi lệnh xem lịch sử
            sendLine(sock, "LIST_MY_HISTORY");
            
            // Nhận danh sách từ server (Server cần trả về chuỗi đã format hoặc danh sách)
            // Giả sử server trả về từng dòng, kết thúc bằng "END_LIST"
            cout << "\n--- YOUR EXAM HISTORY ---\n";
            string resp = recvLine(sock);
            // Format mong đợi từ server: HISTORY|Môn A: 8đ; Môn B: 9đ...
            if (resp.find("HISTORY|") == 0) {
                string data = resp.substr(8); 
                // Tách dấu chấm phẩy để in cho đẹp nếu cần
                auto items = split(data, ';');
                for (auto &i : items) cout << "- " << i << endl;
            } else {
                cout << "Server: " << resp << endl;
            }

        } else if (choice == 4) {
            sendLine(sock, "QUIT");
            cout << "Logging out...\n";
            break;
        } else {
            cout << "Invalid choice.\n";
        }
    }
}

// ==================================================

// MAIN

// ==================================================
 
int main() {

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0) {

        cerr << "Socket creation failed\n";

        return 1;

    }
 
    sockaddr_in addr{};

    addr.sin_family = AF_INET;

    addr.sin_port   = htons(9000);

    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
 
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {

        cerr << "Connection failed\n";

        return 1;

    }
 
    string role = doLogin(sock);

    if (role == "teacher") {

        teacherMenu(sock);

    } else if (role == "student") {

        studentMenu(sock);

    }
 
    close(sock);

    return 0;

}