#include "../include/client.h"
#include <sys/socket.h>
#include <sys/select.h>
#include <sstream>

// Network Helpers
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

bool hasData(int sock, int timeoutSeconds) {
    fd_set readfds;
    struct timeval timeout;
    
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    
    timeout.tv_sec = timeoutSeconds;
    timeout.tv_usec = 0;
    
    // Always pass timeout pointer, even when 0 (immediate return, non-blocking)
    int result = select(sock + 1, &readfds, NULL, NULL, &timeout);
    return result > 0 && FD_ISSET(sock, &readfds);
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
