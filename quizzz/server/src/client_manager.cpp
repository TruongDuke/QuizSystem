#include "../include/client_manager.h"
#include <algorithm>

ClientManager& ClientManager::getInstance() {
    static ClientManager instance;
    return instance;
}

void ClientManager::addClient(int sock) {
    ClientInfo info;
    info.sock = sock;
    info.authenticated = false;
    clients[sock] = info;
}

void ClientManager::removeClient(int sock) {
    clients.erase(sock);
}

ClientInfo* ClientManager::getClient(int sock) {
    auto it = clients.find(sock);
    if (it != clients.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<int> ClientManager::getAllClients() {
    std::vector<int> result;
    for (auto& pair : clients) {
        result.push_back(pair.first);
    }
    return result;
}

std::vector<int> ClientManager::getClientsByRole(const std::string& role) {
    std::vector<int> result;
    for (auto& pair : clients) {
        if (pair.second.authenticated && pair.second.role == role) {
            result.push_back(pair.first);
        }
    }
    return result;
}

void ClientManager::setClientInfo(int sock, const std::string& sessionId,
                                 const std::string& role, 
                                 const std::string& username, int userId) {
    auto it = clients.find(sock);
    if (it != clients.end()) {
        it->second.sessionId = sessionId;
        it->second.role = role;
        it->second.username = username;
        it->second.userId = userId;
        it->second.authenticated = true;
    }
}

