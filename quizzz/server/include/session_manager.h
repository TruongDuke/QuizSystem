#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <map>
#include <vector>

struct StudentSession {
    int examId;
    int currentQuestionIdx;
    float score;
    std::vector<int> questionIds;
};

class SessionManager {
public:
    static SessionManager& getInstance();
    void addSession(int sock, const StudentSession& session);
    void removeSession(int sock);
    StudentSession* getSession(int sock);
    
private:
    SessionManager() {}
    std::map<int, StudentSession> sessions;
};

#endif // SESSION_MANAGER_H

