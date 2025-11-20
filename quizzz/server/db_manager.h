#ifndef DB_MANAGER_H
#define DB_MANAGER_H
 
#include <string>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
 
class DbManager {
public:
    DbManager(const std::string& dbHost,
              const std::string& dbUser,
              const std::string& dbPass,
              const std::string& dbName);
    ~DbManager();
 
    // SELECT -> trả về ResultSet*
    sql::ResultSet* executeQuery(const std::string& query);
 
    // INSERT / UPDATE / DELETE không cần lấy id
    void executeUpdate(const std::string& query);
 
    // INSERT và lấy lại LAST_INSERT_ID()
    int executeInsertAndGetId(const std::string& query);
 
    // Alias cho tương thích code cũ
    int executeInsert(const std::string& query) {
        return executeInsertAndGetId(query);
    }
 
private:
    std::string dbHost;
    std::string dbUser;
    std::string dbPass;
    std::string dbName;
    
    sql::mysql::MySQL_Driver *driver;
    sql::Connection *con; // giữ kết nối ở đây

    // đảm bảo kết nối luôn sẵn sàng
    bool ensureConnection();

public:
    DbManager(const std::string& dbHost, const std::string& dbUser, const std::string& dbPass, const std::string& dbName);
    ~DbManager();

    // === Luồng xác thực ===
    // Trả về user_id nếu thành công, -1 nếu thất bại
    int authenticateStudent(const std::string& username, const std::string& password);

    // Luồng làm bài 

    // Lấy danh sách các bài thi có sẵn
    std::vector<QuizInfo> getAvailableQuizzes();

    // Bắt đầu làm bài: Tạo một "Exam" và trả về exam_id
    // (Trả về -1 nếu có lỗi)
    int startExam(int quizId, int studentId);

    // 3. Lấy toàn bộ câu hỏi và các lựa chọn cho bài thi đó
    std::vector<QuestionInfo> getQuestionsForQuiz(int quizId);

    // 4. Lưu câu trả lời của học sinh vào Exam_Answers
    bool saveStudentAnswer(int examId, int questionId, int answerId);

    // 5. Nộp bài: Chấm điểm, cập nhật Exams, và trả về điểm số
    // (Trả về -1.0 nếu có lỗi)
    float submitAndGradeExam(int examId);
};

#endif // DB_MANAGER_H