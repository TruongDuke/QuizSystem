#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include "db_manager.h" 

void sendMessage(int clientSock, const std::string& message) {
    send(clientSock, message.c_str(), message.length(), 0);
}

// Hàm nhận tin nhắn (helper)
std::string receiveMessage(int clientSock) {
    char buffer[2048] = {0}; // Tăng buffer
    int n = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        return "DISCONNECTED"; // Client ngắt kết nối
    }
    buffer[n] = '\0'; // Đảm bảo là chuỗi null-terminated
    return std::string(buffer);
}

// Tách chuỗi bằng ký tự |
std::vector<std::string> splitCommand(const std::string& str) {
    std::vector<std::string> tokens;
    std::string token;
    size_t start = 0;
    size_t end = str.find('|');
    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + 1;
        end = str.find('|', start);
    }
    tokens.push_back(str.substr(start)); // Lấy phần cuối
    return tokens;
}


//LUỒNG XỬ LÝ CỦA HỌC SINH
void handleStudentSession(int clientSock, DbManager* dbManager, int studentId) {
    std::cout << "Student (ID: " << studentId << ") session started." << std::endl;
    
    // Gửi thông báo cho client biết đã vào luồng học sinh
    sendMessage(clientSock, "SESSION_STUDENT_OK");
    
    int currentExamId = -1; // Lưu lại exam_id đang làm

    while (true) {
        std::string msg = receiveMessage(clientSock);
        if (msg == "DISCONNECTED") {
            std::cout << "Student " << studentId << " disconnected." << std::endl;
            // (Bạn có thể muốn tự động nộp bài nếu họ ngắt kết nối khi đang làm)
            break; 
        }

        std::vector<std::string> command = splitCommand(msg);
        if (command.empty()) continue;

        std::cout << "Student " << studentId << " sent command: " << command[0] << std::endl;

        // --- Xử lý các lệnh từ Client của học sinh ---

        if (command[0] == "GET_QUIZZES") {
            // 1. Client muốn lấy danh sách bài thi
            std::vector<QuizInfo> quizzes = dbManager->getAvailableQuizzes();
            std::string response = "QUIZ_LIST";
            for (const auto& quiz : quizzes) {
                // Định dạng: QUIZ_LIST|id,title,time_limit|id,title,time_limit...
                response += "|" + std::to_string(quiz.id) + "," + quiz.title + "," + std::to_string(quiz.timeLimit);
            }
            sendMessage(clientSock, response);
        
        } else if (command[0] == "START_QUIZ" && command.size() > 1) {
            // 2. Client muốn bắt đầu làm bài
            int quizId = std::stoi(command[1]);
            currentExamId = dbManager->startExam(quizId, studentId);
            
            if (currentExamId == -1) {
                sendMessage(clientSock, "START_EXAM_FAILED");
            } else {
                // Lấy câu hỏi
                std::vector<QuestionInfo> questions = dbManager->getQuestionsForQuiz(quizId);
                
                // Gửi exam_id và toàn bộ câu hỏi/câu trả lời về
                std::string response = "EXAM_DATA|exam_id=" + std::to_string(currentExamId);
                for (const auto& q : questions) {
                    // Q|id,text
                    response += "|Q," + std::to_string(q.id) + "," + q.text;
                    for (const auto& a : q.answers) {
                        // A|id,text
                        response += "|A," + std::to_string(a.id) + "," + a.text;
                    }
                }
                sendMessage(clientSock, response);
            }

        } else if (command[0] == "SUBMIT_ANSWER" && command.size() > 2) {
            // 4. Client nộp MỘT câu trả lời
            int questionId = std::stoi(command[1]);
            int answerId = std::stoi(command[2]);
            
            if (currentExamId != -1) {
                dbManager->saveStudentAnswer(currentExamId, questionId, answerId);
                // Không cần phản hồi, hoặc gửi "ANSWER_SAVED"
            }

        } else if (command[0] == "FINISH_EXAM") {
            // 5. Client nhấn nút "NỘP BÀI"
            if (currentExamId != -1) {
                float score = dbManager->submitAndGradeExam(currentExamId);
                sendMessage(clientSock, "EXAM_FINISHED|score=" + std::to_string(score));
                currentExamId = -1; // Kết thúc phiên
            }
        }
        // ... (Bạn có thể thêm các lệnh khác) ...
    }
}

// Hàm này xử lý Giáo viên 
void handleTeacherSession(int clientSock, DbManager* dbManager, int teacherId) {
    std::cout << "Teacher (ID: " << teacherId << ") session started." << std::endl;
    sendMessage(clientSock, "SESSION_TEACHER_OK");
    // ... (Triển khai logic thêm/sửa/xóa quiz ở đây) ...
   
}


//LUỒNG ĐĂNG NHẬP
void login(int clientSock, DbManager* dbManager) {
    std::string loginData = receiveMessage(clientSock);
    if (loginData == "DISCONNECTED") return;

    // Tách username và password
    std::vector<std::string> parts = splitCommand(loginData); // vd: "LOGIN|user|pass"
    
    // Yêu cầu client gửi: "LOGIN|username|password"
    if (parts.size() != 3 || parts[0] != "LOGIN") {
        sendMessage(clientSock, "LOGIN_FAILED|Invalid format");
        return;
    }
    
    std::string username = parts[1];
    std::string password = parts[2];

    // * GỌI HÀM AN TOÀN *
    // (Không còn SQL Injection, Không còn rò rỉ kết nối)
    int userId = dbManager->authenticateStudent(username, password); // Thử đăng nhập HS
    std::string role = "student";

    if (userId == -1) { // Thử đăng nhập GV hoặc Admin nếu HS thất bại
        // userId = dbManager->authenticateTeacher(username, password); 
        // (Bạn cần tự viết hàm này, tương tự authenticateStudent)
        // Giả sử:
        // if (userId != -1) role = "teacher";
    }

    if (userId != -1) { // Đăng nhập thành công
        std::cout << "Login successful for " << username << " (ID: " << userId << ")" << std::endl;
        
        if (role == "student") {
            handleStudentSession(clientSock, dbManager, userId);
        } else if (role == "teacher") {
            // handleTeacherSession(clientSock, dbManager, userId);
        }
        
    } else {
        std::cout << "Login failed for " << username << std::endl;
        sendMessage(clientSock, "LOGIN_FAILED|Invalid credentials");
    }
}

/* --- HÀM MAIN (KHÔNG THAY ĐỔI NHIỀU) --- */
int main() {
    // Tạo MỘT đối tượng DbManager DUY NHẤT
    DbManager* dbManager = new DbManager("127.0.0.1", "root", "123456", "quizDB");

    // (Code tạo socket, bind, listen của bạn ở đây... )
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    // ... (kiểm tra lỗi) ...
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000); 
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    // ... (kiểm tra lỗi) ...
    listen(serverSock, 5);
    // ... (kiểm tra lỗi) ...
    
    std::cout << "Server is listening on port 9000..." << std::endl;

    while (true) {
        // Chấp nhận kết nối MỚI
        int clientSock = accept(serverSock, nullptr, nullptr);
        if (clientSock < 0) {
            std::cerr << "Accept failed!" << std::endl;
            continue; // Chờ kết nối tiếp theo
        }
        
        std::cout << "New client connected. Waiting for login..." << std::endl;

        // Xử lý đăng nhập
        // (LƯU Ý: Thiết kế này chỉ xử lý 1 client tại 1 thời điểm)
        // (Để xử lý nhiều client, bạn cần dùng thread hoặc fork)
        login(clientSock, dbManager);

        // Sau khi client xử lý xong (ngắt kết nối), đóng socket
        close(clientSock);
        std::cout << "Client session ended." << std::endl;
    }

    // Đóng server
    close(serverSock);
    delete dbManager; // Dọn dẹp DbManager khi server sập
    return 0;
}