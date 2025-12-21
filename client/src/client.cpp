#include "../include/client.h"
#include "../include/quiz_interface.h"
#include "../include/question_interface.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

// ==================================================
// Network Helpers
// ==================================================

std::string recvLine(int sock) {
    std::string line;
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

void sendLine(int sock, const std::string &msg) {
    std::string data = msg + "\n";
    send(sock, data.c_str(), data.size(), 0);
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (getline(ss, item, delim)) out.push_back(item);
    return out;
}

// ==================================================
// Register
// ==================================================

bool doRegister(int sock) {
    std::string username, password, email, role;

    std::cout << "\n=== REGISTER NEW ACCOUNT ===\n";
    std::cout << "Enter username: ";
    std::cin >> username;

    std::cout << "Enter password: ";
    std::cin >> password;

    std::cout << "Enter email: ";
    std::cin >> email;

    std::cout << "Enter role (teacher/student): ";
    std::cin >> role;

    std::string msg = "REGISTER|" + username + "|" + password + "|" + email + "|" + role;
    sendLine(sock, msg);

    std::string resp = recvLine(sock);
    if (resp.empty()) {
        std::cerr << "No response from server\n";
        return false;
    }

    std::cout << "Server: " << resp << std::endl;

    auto parts = split(resp, '|');
    if (parts.size() > 0 && parts[0] == "REGISTER_OK") {
        std::cout << "Registration successful! Please login.\n";
        return true;
    }

    std::cerr << "Registration failed.\n";
    return false;
}

// ==================================================
// Login
// ==================================================

std::string doLogin(int sock) {
    std::string username, password;

    std::cout << "Enter username: ";
    std::cin >> username;

    std::cout << "Enter password: ";
    std::cin >> password;

    std::string msg = "LOGIN|" + username + "|" + password;
    sendLine(sock, msg);

    std::string resp = recvLine(sock);
    if (resp.empty()) {
        std::cerr << "No response from server\n";
        return "";
    }

    std::cout << "Server: " << resp << std::endl;

    auto parts = split(resp, '|');
    if (parts.size() == 0) return "";

    if (parts[0] == "LOGIN_OK") {
        std::string role = "";
        for (auto &p : parts) {
            if (p.rfind("role=", 0) == 0) {
                role = p.substr(5);
            }
        }
        std::cout << "Logged in as: " << role << std::endl;
        return role;
    }

    std::cerr << "Login failed.\n";
    return "";
}

// ==================================================
// MAIN
// ==================================================

int main(int argc, char** argv) {
    std::cout << "\n=== QUIZ SYSTEM ===\n";
    std::cout << "1. Login\n";
    std::cout << "2. Register\n";
    std::cout << "Choose option: ";
    std::cout.flush();
    
    int choice;
    std::cin >> choice;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

    // Server IP & port from CLI args: ./quiz_client <IP> [PORT]
    std::string ipStr = (argc >= 2 ? std::string(argv[1]) : std::string("127.0.0.1"));
    int port = 9000;
    if (argc >= 3) {
        try {
            port = std::stoi(argv[2]);
        } catch (...) {
            std::cerr << "Invalid port. Usage: ./quiz_client <IP> [PORT]" << std::endl;
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
        return 1;
    }

    if (choice == 2) {
        // Register
        if (doRegister(sock)) {
            std::cout << "\nPlease restart the client and login.\n";
        }
        close(sock);
        return 0;
    }

    std::string role = doLogin(sock);

    if (role == "teacher") {
        teacherMenu(sock);
    } else if (role == "student") {
        studentMenu(sock);
    }

    close(sock);
    return 0;
}

