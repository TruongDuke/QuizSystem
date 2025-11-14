#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>  // Bao gồm thư viện unistd.h để sử dụng close()
 
// Khai báo hàm manageQuiz trước khi gọi
void manageQuiz(int sock);
 
void login(int sock) {
    std::string username;
    std::string password;
 
    // Nhận username và password từ bàn phím
    std::cout << "Enter username: ";
    std::cin >> username;
    std::cout << "Enter password: ";
    std::cin >> password;
 
    // Gửi yêu cầu đăng nhập tới server, dùng ký tự "|" để phân tách
    std::string loginMessage = username + "|" + password;
    int sent = send(sock, loginMessage.c_str(), loginMessage.length(), 0);
    if (sent < 0) {
        std::cerr << "Error sending message to server!" << std::endl;
        return;
    }
 
    // Nhận phản hồi từ server
    char buffer[1024];
    int n = recv(sock, buffer, sizeof(buffer), 0);
    if (n < 0) {
        std::cerr << "Error receiving message from server!" << std::endl;
        return;
    }
 
    std::string response(buffer, n);
    std::cout << "Server response: " << response << std::endl;
 
    // Phân tích phản hồi và xác định role
    size_t pos = response.find("|role=");
    if (pos != std::string::npos) {
        std::string role = response.substr(pos + 5);  // Lấy phần role từ phản hồi
        if (role == "teacher") {
            std::cout << "You are logged in as a Teacher." << std::endl;
            // Mở giao diện chức năng cho giáo viên (thêm, sửa, xóa kỳ thi)
            manageQuiz(sock); // Ví dụ: thêm, sửa kỳ thi
        } else if (role == "student") {
            std::cout << "You are logged in as a Student." << std::endl;
            // Mở giao diện chức năng cho học sinh (tham gia kỳ thi)
             // Ví dụ: tham gia kỳ thi
        }
    }
}
 
// Định nghĩa hàm manageQuiz
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
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation failed!" << std::endl;
        return 1;
    }
 
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000);  // Cổng mà server lắng nghe
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Kết nối vào localhost
 
    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed!" << std::endl;
        return 1;
    }
 
    // Gửi yêu cầu đăng nhập tới server
    login(sock);
 
    // Sau khi đăng nhập thành công, cho phép quản lý kỳ thi nếu là giáo viên
    manageQuiz(sock);
 
    // Đóng kết nối socket
    close(sock);  // Đảm bảo close() được gọi để đóng kết nối với server
    return 0;
}