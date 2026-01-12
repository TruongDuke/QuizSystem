#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <vector>

// Network helpers
std::string recvLine(int sock);
bool hasData(int sock, int timeoutSeconds = 0); // Check if socket has data (non-blocking)
void sendLine(int sock, const std::string &msg);

// String utilities
std::vector<std::string> split(const std::string &s, char delim);

// Login
std::string doLogin(int sock);

#endif // CLIENT_H

