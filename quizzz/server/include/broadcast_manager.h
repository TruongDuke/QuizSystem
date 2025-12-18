#ifndef BROADCAST_MANAGER_H
#define BROADCAST_MANAGER_H

#include <string>

class BroadcastManager {
public:
    static BroadcastManager& getInstance();
    
    // Broadcast to all authenticated clients
    void broadcast(const std::string& message);
    
    // Broadcast to specific role
    void broadcastToRole(const std::string& role, const std::string& message);
    
    // Send to specific client
    void sendToClient(int sock, const std::string& message);
    
private:
    BroadcastManager() {}
};

#endif // BROADCAST_MANAGER_H

