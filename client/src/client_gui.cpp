#include "../include/client.h"
#include "../include/quiz_interface.h"
#include "../include/question_interface.h"
#include "../include/gui/gui_login.h"
#include "../include/gui/student/gui_room_list.h"
#include "../include/gui/student/gui_exam.h"
#include "../include/gui/student/gui_history.h"
#include "../include/gui/teacher/gui_teacher_main.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>

int main(int argc, char** argv) {
    // Vòng lặp chính để cho phép đăng nhập lại sau khi logout
    while (true) {
        // Hiển thị màn hình Login GUI
        LoginResult loginResult = showLoginWindow();
        
        if (!loginResult.success) {
            std::cout << "User cancelled login\n";
            return 0;
        }
        
        // Kết nối đến server
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            std::cerr << "Socket creation failed\n";
            return 1;
        }
        
        // Server IP & port từ CLI args hoặc mặc định
        std::string ipStr = (argc >= 2 ? std::string(argv[1]) : std::string("127.0.0.1"));
        int port = 9000;
        if (argc >= 3) {
            try {
                port = std::stoi(argv[2]);
            } catch (...) {
                std::cerr << "Invalid port. Usage: ./quiz_client_gui <IP> [PORT]" << std::endl;
                close(sock);
                return 1;
            }
        }
        
        std::cout << "Connecting to " << ipStr << ":" << port << "..." << std::endl;
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        addr.sin_addr.s_addr = inet_addr(ipStr.c_str());
        
        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "Connection failed to " << ipStr << ":" << port << "\n";
            close(sock);
            continue; // Thử lại từ màn hình login
        }
        
        std::string role;
        
        // Xử lý Register hoặc Login
        if (loginResult.isRegister) {
            // Gửi request Register
            std::string msg = "REGISTER|" + loginResult.username + "|" + 
                             loginResult.password + "|" + loginResult.email + "|" + 
                             loginResult.role;
            sendLine(sock, msg);
            
            std::string resp = recvLine(sock);
            std::cout << "Server: " << resp << std::endl;
            
            auto parts = split(resp, '|');
            if (parts.size() > 0 && parts[0] == "REGISTER_OK") {
                std::cout << "Registration successful! Please login again.\n";
                close(sock);
                continue; // Quay lại màn hình login
            } else {
                std::cerr << "Registration failed.\n";
                close(sock);
                continue; // Quay lại màn hình login
            }
        } else {
            // Gửi request Login
            std::string msg = "LOGIN|" + loginResult.username + "|" + loginResult.password;
            sendLine(sock, msg);
            
            std::string resp = recvLine(sock);
            std::cout << "Server: " << resp << std::endl;
            
            auto parts = split(resp, '|');
            if (parts.size() == 0 || parts[0] != "LOGIN_OK") {
                std::cerr << "Login failed.\n";
                close(sock);
                continue; // Quay lại màn hình login
            }
            
            // Lấy role từ response
            for (auto &p : parts) {
                if (p.rfind("role=", 0) == 0) {
                    role = p.substr(5);
                }
            }
            
            std::cout << "Logged in as: " << role << std::endl;
        }
        
        // Chuyển sang menu tương ứng
        if (role == "teacher") {
            // Hiển thị Teacher Dashboard GUI
            std::cout << "Opening Teacher Dashboard...\n";
            showTeacherDashboard(sock);
            close(sock);
            continue; // Quay lại màn hình login
        } else if (role == "student") {
            // Hiển thị GUI danh sách phòng thi
            bool logout = false;
            while (!logout) {
                RoomListResult roomResult = showRoomListWindow(sock);
                
                if (!roomResult.selectedRoom) {
                    // User clicked Logout
                    std::cout << "User logged out\n";
                    logout = true;
                    break;
                }
                
                // Vào phòng thi bằng GUI
                std::cout << "\n=== Entering exam room: " << roomResult.title << " ===\n";
                std::cout << "Exam ID: " << roomResult.examId << "\n";
                
                ExamResult examResult = showExamWindow(sock, roomResult.examId, roomResult.title);
                
                if (examResult.completed) {
                    std::cout << "Exam completed. Score: " << examResult.score << std::endl;
                }
                
                // Sau khi thi xong, quay lại danh sách phòng
            }
            close(sock);
            // Sau khi logout, vòng lặp while(true) sẽ quay lại màn hình login
        }
    }
    
    return 0;
}
