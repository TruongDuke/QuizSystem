#include "db_manager.h"
#include <iostream>

// --- PHẦN QUẢN LÝ KẾT NỐI ---

DbManager::DbManager(const std::string& dbHost, const std::string& dbUser, const std::string& dbPass, const std::string& dbName)
    : dbHost(dbHost), dbUser(dbUser), dbPass(dbPass), dbName(dbName), con(nullptr) {
    try {
        driver = sql::mysql::get_mysql_driver_instance();
    } catch (sql::SQLException &e) {
        std::cerr << "Could not get MySQL driver instance: " << e.what() << std::endl;
        driver = nullptr;
    }
}

DbManager::~DbManager() {
    if (con) {
        delete con;
        con = nullptr;
    }
}

// Hàm nội bộ để kiểm tra và tái kết nối nếu cần
bool DbManager::ensureConnection() {
    if (!driver) return false;
    try {
        if (con && !con->isClosed()) {
            return true; // Vẫn còn kết nối
        }
        
        // Xóa kết nối cũ (nếu bị ngắt)
        if (con) delete con;

        // Tạo kết nối mới
        con = driver->connect("tcp://" + dbHost + ":3306", dbUser, dbPass);
        if (!con) {
             std::cerr << "Failed to connect to database." << std::endl;
             return false;
        }
        con->setSchema(dbName);
        return true;

    } catch (sql::SQLException &e) {
        std::cerr << "Connection Error: " << e.what() << std::endl;
        if (con) delete con;
        con = nullptr;
        return false;
    }
}


// --- PHẦN NGHIỆP VỤ ---

int DbManager::authenticateStudent(const std::string& username, const std::string& password) {
    if (!ensureConnection()) return -1; // -1 là lỗi

    sql::PreparedStatement *pstmt = nullptr;
    sql::ResultSet *res = nullptr;
    try {
        pstmt = con->prepareStatement("SELECT user_id FROM Users WHERE username = ? AND password = ? AND role = 'student'");
        pstmt->setString(1, username);
        pstmt->setString(2, password);

        res = pstmt->executeQuery();
        int userId = -1; // -1 là sai
        if (res->next()) {
            userId = res->getInt("user_id");
        }
        
        delete res;
        delete pstmt;
        return userId;

    } catch (sql::SQLException &e) {
        std::cerr << "SQL Error (authenticateStudent): " << e.what() << std::endl;
        if(res) delete res;
        if(pstmt) delete pstmt;
        return -1; // -1 là lỗi
    }
}


// 1. Lấy danh sách bài thi
std::vector<QuizInfo> DbManager::getAvailableQuizzes() {
    std::vector<QuizInfo> quizzes;
    if (!ensureConnection()) return quizzes;

    sql::PreparedStatement *pstmt = nullptr;
    sql::ResultSet *res = nullptr;
    try {
        pstmt = con->prepareStatement("SELECT quiz_id, title, description, time_limit FROM Quizzes WHERE status = 'not_started'");
        res = pstmt->executeQuery();

        while (res->next()) {
            QuizInfo quiz;
            quiz.id = res->getInt("quiz_id");
            quiz.title = res->getString("title");
            quiz.description = res->getString("description");
            quiz.timeLimit = res->getInt("time_limit");
            quizzes.push_back(quiz);
        }
        
        delete res;
        delete pstmt;
        return quizzes;

    } catch (sql::SQLException &e) {
        std::cerr << "SQL Error (getAvailableQuizzes): " << e.what() << std::endl;
        if(res) delete res;
        if(pstmt) delete pstmt;
        return quizzes; // Trả về vector rỗng nếu lỗi
    }
}

// 2. Bắt đầu làm bài (Tạo Exam)
int DbManager::startExam(int quizId, int studentId) {
    if (!ensureConnection()) return -1; // Hoặc một giá trị lỗi nào đó

    sql::PreparedStatement *pstmt = nullptr;
    sql::ResultSet *res = nullptr; // Đổi tên để tránh nhầm lẫn, dùng cho ID
    sql::Statement *stmt = nullptr; // Dùng để lấy ID

    int examId = 0; // Giá trị trả về, 0 nghĩa là thất bại

    try {
        // 1. Chèn bản ghi Exam mới
        pstmt = con->prepareStatement("INSERT INTO Exams (quiz_id, user_id, start_time) VALUES (?, ?, NOW())");
        pstmt->setInt(1, quizId);
        pstmt->setInt(2, studentId);
        pstmt->executeUpdate(); // Thực thi chèn

        // 2. Lấy ID vừa được chèn
        stmt = con->createStatement();
        res = stmt->executeQuery("SELECT LAST_INSERT_ID()");

        if (res->next()) {
            examId = res->getInt(1); // Lấy ID từ cột đầu tiên
        }

        // 3. Dọn dẹp tài nguyên
        delete res;
        delete stmt;
        delete pstmt;

    } catch (sql::SQLException &e) {
        // Xử lý ngoại lệ
        std::cout << "# ERR: SQLException in " << __FILE__;
        std::cout << " (line " << __LINE__ << ")" << std::endl;
        std::cout << "# ERR: " << e.what();
        std::cout << " (MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;

        // Dọn dẹp tài nguyên ngay cả khi có lỗi
        if (res) delete res;
        if (stmt) delete stmt;
        if (pstmt) delete pstmt;

        return 0; // Trả về 0 hoặc -1 để báo lỗi
    }

    return examId; // Trả về ID của exam vừa tạo
}

// 3. Lấy câu hỏi
std::vector<QuestionInfo> DbManager::getQuestionsForQuiz(int quizId) {
    std::vector<QuestionInfo> questions;
    if (!ensureConnection()) return questions;

    sql::PreparedStatement *pstmtQuestions = nullptr;
    sql::ResultSet *resQuestions = nullptr;
    sql::PreparedStatement *pstmtAnswers = nullptr;
    sql::ResultSet *resAnswers = nullptr;

    try {
        // Bước 1: Lấy tất cả câu hỏi
        pstmtQuestions = con->prepareStatement("SELECT question_id, question_text FROM Questions WHERE quiz_id = ?");
        pstmtQuestions->setInt(1, quizId);
        resQuestions = pstmtQuestions->executeQuery();

        // Chuẩn bị sẵn câu lệnh lấy câu trả lời
        pstmtAnswers = con->prepareStatement("SELECT answer_id, answer_text FROM Answers WHERE question_id = ?");

        while (resQuestions->next()) {
            QuestionInfo q;
            q.id = resQuestions->getInt("question_id");
            q.text = resQuestions->getString("question_text");

            // Bước 2: Với mỗi câu hỏi, lấy các lựa chọn trả lời (KHÔNG lấy is_correct)
            pstmtAnswers->setInt(1, q.id);
            resAnswers = pstmtAnswers->executeQuery();
            while (resAnswers->next()) {
                AnswerInfo a;
                a.id = resAnswers->getInt("answer_id");
                a.text = resAnswers->getString("answer_text");
                q.answers.push_back(a);
            }
            delete resAnswers; // Quan trọng: Dọn dẹp result set bên trong vòng lặp

            questions.push_back(q);
        }

        // Dọn dẹp chính
        delete resQuestions;
        delete pstmtQuestions;
        delete pstmtAnswers;
        
        return questions;

    } catch (sql::SQLException &e) {
        std::cerr << "SQL Error (getQuestionsForQuiz): " << e.what() << std::endl;
        if(resQuestions) delete resQuestions;
        if(pstmtQuestions) delete pstmtQuestions;
        if(resAnswers) delete resAnswers;
        if(pstmtAnswers) delete pstmtAnswers;
        return questions; // Trả về rỗng nếu lỗi
    }
}

// 4. Lưu một câu trả lời
bool DbManager::saveStudentAnswer(int examId, int questionId, int answerId) {
    if (!ensureConnection()) return false;
    sql::PreparedStatement *pstmt = nullptr;
    try {
        // Dùng REPLACE INTO để nếu học sinh đổi ý, câu trả lời cũ sẽ bị ghi đè
        pstmt = con->prepareStatement("REPLACE INTO Exam_Answers (exam_id, question_id, answer_id) VALUES (?, ?, ?)");
        pstmt->setInt(1, examId);
        pstmt->setInt(2, questionId);
        pstmt->setInt(3, answerId);
        
        pstmt->executeUpdate();
        
        delete pstmt;
        return true;

    } catch (sql::SQLException &e) {
        std::cerr << "SQL Error (saveStudentAnswer): " << e.what() << std::endl;
        if(pstmt) delete pstmt;
        return false;
    }
}

// 5. Nộp bài và chấm điểm
float DbManager::submitAndGradeExam(int examId) {
    if (!ensureConnection()) return -1.0;

    sql::PreparedStatement *pstmtCount = nullptr;
    sql::ResultSet *resCount = nullptr;
    sql::PreparedStatement *pstmtUpdate = nullptr;

    try {
        // Bước 1: Đếm số câu trả lời đúng
        // Đây là truy vấn phức tạp nhất, JOIN 3 bảng
        // 1. Lấy quiz_id từ Exams
        // 2. Lấy các câu trả lời đã chọn (Exam_Answers)
        // 3. So sánh với các câu trả lời đúng (Answers)
        pstmtCount = con->prepareStatement(
            "SELECT COUNT(a.answer_id) AS correct_count "
            "FROM Exam_Answers ea "
            "JOIN Answers a ON ea.answer_id = a.answer_id "
            "WHERE ea.exam_id = ? AND a.is_correct = 1" // is_correct = TRUE
        );
        pstmtCount->setInt(1, examId);
        
        resCount = pstmtCount->executeQuery();
        int correctCount = 0;
        if (resCount->next()) {
            correctCount = resCount->getInt("correct_count");
        }
        delete resCount;
        delete pstmtCount;
        
        // (Bạn có thể cần lấy tổng số câu hỏi từ bảng Quizzes để tính điểm %
        //  nhưng ở đây chúng ta chỉ giả định điểm là số câu đúng)
        float finalScore = (float)correctCount; 

        // Bước 2: Cập nhật điểm và trạng thái trong bảng Exams
        pstmtUpdate = con->prepareStatement("UPDATE Exams SET end_time = NOW(), score = ?, status = 'submitted' WHERE exam_id = ?");
        pstmtUpdate->setDouble(1, finalScore);
        pstmtUpdate->setInt(2, examId);
        
        pstmtUpdate->executeUpdate();
        
        delete pstmtUpdate;
        
        return finalScore; // Trả về điểm

    } catch (sql::SQLException &e) {
        std::cerr << "SQL Error (submitAndGradeExam): " << e.what() << std::endl;
        if(resCount) delete resCount;
        if(pstmtCount) delete pstmtCount;
        if(pstmtUpdate) delete pstmtUpdate;
        return -1.0; // Lỗi
    }
}