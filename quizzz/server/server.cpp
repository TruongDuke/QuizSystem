#include <iostream>
#include <string>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "db_manager.h"
 
void sendMessage(int clientSock, const std::string& message) {
    // Gửi thông điệp qua socket tới client
    int n = send(clientSock, message.c_str(), message.length(), 0);
    if (n < 0) {
        std::cerr << "Error sending message to client!" << std::endl;
    }
}
 
void login(int clientSock, DbManager* dbManager) {
    // Nhận thông tin đăng nhập từ client
    char buffer[1024];
    int n = recv(clientSock, buffer, sizeof(buffer), 0);
    std::string loginData(buffer, n);
 
    // Tách username và password từ dữ liệu nhận được
    size_t pos = loginData.find("|");
    std::string username = loginData.substr(0, pos);
    std::string password = loginData.substr(pos + 1);
 
    // Truy vấn cơ sở dữ liệu để kiểm tra tài khoản
    std::string query = "SELECT * FROM Users WHERE username = '" + username + "' AND password = '" + password + "';";
    sql::ResultSet* res = dbManager->executeQuery(query);
 
    if (res && res->next()) {
        std::string role = res->getString("role");  // Lấy role (admin, teacher, student)
        std::string sessionId = "S123"; // Tạo sessionId cho client
 
        std::string response = "LOGIN_OK|sessionId=" + sessionId + "|role=" + role;
        sendMessage(clientSock, response);  // Gửi thông báo đăng nhập thành công cùng role
 
        // Phân biệt giữa giáo viên và học sinh
        if (role == "teacher") {
            // Chức năng của giáo viên (Thêm, sửa, xóa kỳ thi, câu hỏi)
            std::cout << "Teacher logged in" << std::endl;
            // Gửi thông báo hoặc điều hướng vào chế độ giáo viên
        } else if (role == "student") {
            // Chức năng của học sinh (Tham gia thi, trả lời câu hỏi)
            std::cout << "Student logged in" << std::endl;
            // Gửi thông báo hoặc điều hướng vào chế độ học sinh
        }
    } else {
        std::string response = "LOGIN_FAILED";
        sendMessage(clientSock, response);  // Gửi thông báo đăng nhập thất bại
    }
}
 
void addQuiz(int clientSock, DbManager* dbManager) {
    char buffer[1024];
    int n = recv(clientSock, buffer, sizeof(buffer), 0);
    std::string quizData(buffer, n);
 
    // Tách thông tin từ client gửi tới (title, description, question_count, time_limit)
    size_t pos1 = quizData.find("|");
    std::string title = quizData.substr(0, pos1);
    size_t pos2 = quizData.find("|", pos1 + 1);
    std::string description = quizData.substr(pos1 + 1, pos2 - pos1 - 1);
    size_t pos3 = quizData.find("|", pos2 + 1);
    int question_count = std::stoi(quizData.substr(pos2 + 1, pos3 - pos2 - 1));
    int time_limit = std::stoi(quizData.substr(pos3 + 1));
 
    // Truy vấn để thêm kỳ thi vào cơ sở dữ liệu
    std::string query = "INSERT INTO Quizzes (title, description, question_count, time_limit, status, creator_id) "
                        "VALUES ('" + title + "', '" + description + "', " + std::to_string(question_count) + ", " + std::to_string(time_limit) + ", 'not_started', 2);";
    dbManager->executeUpdate(query);
 
    std::string response = "Quiz added successfully!";
    sendMessage(clientSock, response);  // Gửi phản hồi thành công
}
 
void editQuiz(int clientSock, DbManager* dbManager) {
    char buffer[1024];
    int n = recv(clientSock, buffer, sizeof(buffer), 0);
    std::string quizData(buffer, n);
 
    // Tách thông tin từ client gửi tới (quiz_id, title, description, question_count, time_limit)
    size_t pos1 = quizData.find("|");
    int quiz_id = std::stoi(quizData.substr(0, pos1));
    size_t pos2 = quizData.find("|", pos1 + 1);
    std::string title = quizData.substr(pos1 + 1, pos2 - pos1 - 1);
    size_t pos3 = quizData.find("|", pos2 + 1);
    std::string description = quizData.substr(pos2 + 1, pos3 - pos2 - 1);
    size_t pos4 = quizData.find("|", pos3 + 1);
    int question_count = std::stoi(quizData.substr(pos3 + 1, pos4 - pos3 - 1));
    int time_limit = std::stoi(quizData.substr(pos4 + 1));
 
    // Truy vấn để sửa thông tin kỳ thi trong cơ sở dữ liệu
    std::string query = "UPDATE Quizzes SET title = '" + title + "', description = '" + description + "', "
                        "question_count = " + std::to_string(question_count) + ", time_limit = " + std::to_string(time_limit) +
                        " WHERE quiz_id = " + std::to_string(quiz_id);
    dbManager->executeUpdate(query);
 
    std::string response = "Quiz updated successfully!";
    sendMessage(clientSock, response);  // Gửi phản hồi thành công
}
 
void deleteQuiz(int clientSock, DbManager* dbManager) {
    char buffer[1024];
    int n = recv(clientSock, buffer, sizeof(buffer), 0);
    std::string quiz_id(buffer, n);
 
    // Truy vấn để xóa kỳ thi khỏi cơ sở dữ liệu
    std::string query = "DELETE FROM Quizzes WHERE quiz_id = " + quiz_id;
    dbManager->executeUpdate(query);
 
    std::string response = "Quiz deleted successfully!";
    sendMessage(clientSock, response);  // Gửi phản hồi thành công
}

void manageQuiz(int sock) {
    int choice;
    std::cout << "Choose an action:\n";
    std::cout << "1. Add Quiz\n";
    std::cout << "2. Edit Quiz\n";
    std::cout << "3. Delete Quiz\n";
    std::cout << "Enter your choice: ";
    std::cin >> choice;
 
    if (choice == 1) {
        // Add Quiz
        std::string title, description;
        int question_count, time_limit;
 
        std::cout << "Enter quiz title: ";
        std::cin.ignore();
        std::getline(std::cin, title);
        std::cout << "Enter quiz description: ";
        std::getline(std::cin, description);
        std::cout << "Enter question count: ";
        std::cin >> question_count;
        std::cout << "Enter time limit (in seconds): ";
        std::cin >> time_limit;
 
        std::string quizData = title + "|" + description + "|" + std::to_string(question_count) + "|" + std::to_string(time_limit);
        send(sock, quizData.c_str(), quizData.length(), 0);
 
    } else if (choice == 2) {
        // Edit Quiz
        int quiz_id;
        std::string title, description;
        int question_count, time_limit;
 
        std::cout << "Enter quiz ID to edit: ";
        std::cin >> quiz_id;
        std::cout << "Enter new quiz title: ";
        std::cin.ignore();
        std::getline(std::cin, title);
        std::cout << "Enter new quiz description: ";
        std::getline(std::cin, description);
        std::cout << "Enter new question count: ";
        std::cin >> question_count;
        std::cout << "Enter new time limit (in seconds): ";
        std::cin >> time_limit;
 
        std::string quizData = std::to_string(quiz_id) + "|" + title + "|" + description + "|" + std::to_string(question_count) + "|" + std::to_string(time_limit);
        send(sock, quizData.c_str(), quizData.length(), 0);
 
    } else if (choice == 3) {
        // Delete Quiz
        int quiz_id;
        std::cout << "Enter quiz ID to delete: ";
        std::cin >> quiz_id;
 
        send(sock, std::to_string(quiz_id).c_str(), std::to_string(quiz_id).length(), 0);
    }
}
 
 
int main() {
    // Tạo kết nối cơ sở dữ liệu
    DbManager* dbManager = new DbManager("127.0.0.1", "root", "123456", "quizDB");
 
    // Tạo một socket TCP
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) {
        std::cerr << "Error creating socket!" << std::endl;
        return 1;
    }
 
    // Đặt địa chỉ server (IP, cổng)
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000);  // Cổng mà server lắng nghe
    serverAddr.sin_addr.s_addr = INADDR_ANY;
 
    // Liên kết socket với địa chỉ server
    if (bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Bind failed!" << std::endl;
        return 1;
    }
 
    // Lắng nghe kết nối từ client
    if (listen(serverSock, 5) < 0) {
        std::cerr << "Listen failed!" << std::endl;
        return 1;
    }
 
    std::cout << "Server is listening on port 9000..." << std::endl;
 
    // Chấp nhận kết nối từ client
    int clientSock = accept(serverSock, nullptr, nullptr);
    if (clientSock < 0) {
        std::cerr << "Accept failed!" << std::endl;
        return 1;
    }
 
    // Gọi hàm login để xử lý đăng nhập
    login(clientSock, dbManager);
 
    // Xử lý các yêu cầu quản lý kỳ thi
    manageQuiz(clientSock);
 
    // Đóng kết nối
    close(clientSock);
    close(serverSock);
    delete dbManager;
 
    return 0;
}