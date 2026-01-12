#include "../include/broadcast_manager.h"
#include "../include/client_manager.h"
#include "../include/protocol_manager.h"


BroadcastManager& BroadcastManager::getInstance() {
    static BroadcastManager instance;
    return instance;
}

void BroadcastManager::broadcast(const std::string& message) {
    ClientManager& clientMgr = ClientManager::getInstance();
    auto clients = clientMgr.getAllClients();
    
    for (int sock : clients) {
        ClientInfo* info = clientMgr.getClient(sock);
        if (info && info->authenticated) {
            sendLine(sock, message);
        }
    }
}

void BroadcastManager::broadcastToRole(const std::string& role, const std::string& message) {
    ClientManager& clientMgr = ClientManager::getInstance();
    auto clients = clientMgr.getClientsByRole(role);
    
    for (int sock : clients) {
        sendLine(sock, message);
    }
}

void BroadcastManager::sendToClient(int sock, const std::string& message) {
    sendLine(sock, message);
}

