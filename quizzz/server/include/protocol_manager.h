#ifndef PROTOCOL_MANAGER_H
#define PROTOCOL_MANAGER_H

#include <string>
#include <vector>

std::string recvLine(int sock);
void sendLine(int sock, const std::string &msg);
std::vector<std::string> split(const std::string &s, char delim);
std::string escapeSql(const std::string &s);

#endif // PROTOCOL_MANAGER_H

