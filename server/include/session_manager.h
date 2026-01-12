#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <string>
#include <map>

struct Session {
    std::string sessionId;
    int userId;
    std::string username;
    std::string role;
};

class SessionManager {
public:
    static SessionManager& getInstance();
    
    std::string createSession(int userId, const std::string& username, const std::string& role);
    bool validateSession(const std::string& sessionId);
    Session* getSession(const std::string& sessionId);
    void removeSession(const std::string& sessionId);
    
private:
    SessionManager() {}
    std::map<std::string, Session> sessions;
    int sessionCounter = 0;
};

#endif // SESSION_MANAGER_H

