#include "../include/protocol_manager.h"
#include <sys/socket.h>
#include <sstream>
#include <iostream>

std::string recvLine(int sock) {
    std::string line;
    char c;
    while (true) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) return "";
        if (c == '\n') break;
        line.push_back(c);
    }
    return line;
}

void sendLine(int sock, const std::string &msg) {
    std::string data = msg + "\n";
    int n = send(sock, data.c_str(), data.size(), 0);
    if (n < 0) {
        std::cerr << "[NET] Error sending data\n";
    }
}

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (getline(ss, item, delim)) out.push_back(item);
    return out;
}

std::string escapeSql(const std::string &s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '\'') out += "''";
        else out.push_back(c);
    }
    return out;
}

