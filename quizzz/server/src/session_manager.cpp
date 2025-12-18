#include "../include/session_manager.h"
#include <sstream>
#include <ctime>

SessionManager& SessionManager::getInstance() {
    static SessionManager instance;
    return instance;
}

std::string SessionManager::createSession(int userId, const std::string& username, const std::string& role) {
    std::stringstream ss;
    ss << "S" << (++sessionCounter) << "_" << time(nullptr);
    std::string sessionId = ss.str();
    
    Session session;
    session.sessionId = sessionId;
    session.userId = userId;
    session.username = username;
    session.role = role;
    
    sessions[sessionId] = session;
    return sessionId;
}

bool SessionManager::validateSession(const std::string& sessionId) {
    return sessions.find(sessionId) != sessions.end();
}

Session* SessionManager::getSession(const std::string& sessionId) {
    auto it = sessions.find(sessionId);
    if (it != sessions.end()) {
        return &it->second;
    }
    return nullptr;
}

void SessionManager::removeSession(const std::string& sessionId) {
    sessions.erase(sessionId);
}

