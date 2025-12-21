#ifndef PROTOCOL_MANAGER_H
#define PROTOCOL_MANAGER_H

#include <string>
#include <vector>

// Network communication helpers
std::string recvLine(int sock);
void sendLine(int sock, const std::string &msg);

// String utilities
std::vector<std::string> split(const std::string &s, char delim);
std::string escapeSql(const std::string &s);

#endif // PROTOCOL_MANAGER_H

