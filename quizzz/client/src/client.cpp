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

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(9000);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Connection failed\n";
        return 1;
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

