#ifndef CLIENT_MANAGER_H
#define CLIENT_MANAGER_H

#include <map>
#include <string>
#include <vector>

struct ClientInfo {
    int sock;
    std::string sessionId;
    std::string role;
    std::string username;
    int userId;
    bool authenticated;
    // Exam state tracking
    int currentExamId;        // Exam ID đang làm (0 = không có exam)
    int currentQuestionIndex; // Index câu hỏi hiện tại (0-based)
    std::vector<int> questionIds; // Danh sách question IDs của exam
};

class ClientManager {
public:
    static ClientManager& getInstance();
    
    void addClient(int sock);
    void removeClient(int sock);
    ClientInfo* getClient(int sock);
    std::vector<int> getAllClients();
    std::vector<int> getClientsByRole(const std::string& role);
    void setClientInfo(int sock, const std::string& sessionId, 
                      const std::string& role, const std::string& username, int userId);
    
private:
    ClientManager() {}
    std::map<int, ClientInfo> clients;
};

#endif // CLIENT_MANAGER_H

