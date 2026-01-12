
-- Tạo database quizDB
CREATE DATABASE IF NOT EXISTS quizDB;

USE quizDB;

-- Tạo bảng Users
CREATE TABLE IF NOT EXISTS Users (
    user_id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(255) NOT NULL,
    password VARCHAR(255) NOT NULL,
    email VARCHAR(255) NOT NULL,
    role ENUM('admin', 'teacher', 'student') NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Tạo bảng Quizzes (với scheduled exam support)
CREATE TABLE IF NOT EXISTS Quizzes (
    quiz_id INT AUTO_INCREMENT PRIMARY KEY,
    title VARCHAR(255) NOT NULL,
    description TEXT,
    question_count INT,
    time_limit INT,
    creator_id INT,
    status ENUM('not_started', 'in_progress', 'finished') NOT NULL,
    exam_type ENUM('normal', 'scheduled') DEFAULT 'normal',
    exam_start_time DATETIME NULL,
    exam_end_time DATETIME NULL,
    FOREIGN KEY (creator_id) REFERENCES Users(user_id)
);

-- Tạo bảng Questions
CREATE TABLE IF NOT EXISTS Questions (
    question_id INT AUTO_INCREMENT PRIMARY KEY,
    quiz_id INT,
    question_text TEXT NOT NULL,
    difficulty ENUM('easy', 'medium', 'hard') NOT NULL,
    topic VARCHAR(255),
    FOREIGN KEY (quiz_id) REFERENCES Quizzes(quiz_id)
);

-- Tạo bảng Answers
CREATE TABLE IF NOT EXISTS Answers (
    answer_id INT AUTO_INCREMENT PRIMARY KEY,
    question_id INT,
    answer_text TEXT NOT NULL,
    is_correct BOOLEAN NOT NULL,
    FOREIGN KEY (question_id) REFERENCES Questions(question_id)
);

-- Tạo bảng Exams
CREATE TABLE IF NOT EXISTS Exams (
    exam_id INT AUTO_INCREMENT PRIMARY KEY,
    quiz_id INT,
    user_id INT,
    start_time DATETIME,
    end_time DATETIME,
    score FLOAT,
    status ENUM('in_progress', 'submitted') NOT NULL,
    FOREIGN KEY (quiz_id) REFERENCES Quizzes(quiz_id),
    FOREIGN KEY (user_id) REFERENCES Users(user_id)
);

-- Tạo bảng Exam_Answers
CREATE TABLE IF NOT EXISTS Exam_Answers (
    exam_answer_id INT AUTO_INCREMENT PRIMARY KEY,
    exam_id INT,
    question_id INT,
    answer_id INT,
    chosen_answer TEXT,
    FOREIGN KEY (exam_id) REFERENCES Exams(exam_id),
    FOREIGN KEY (question_id) REFERENCES Questions(question_id),
    FOREIGN KEY (answer_id) REFERENCES Answers(answer_id)
);

-- Tạo bảng QuestionBank (ngân hàng câu hỏi chung, không gắn với quiz cụ thể)
CREATE TABLE IF NOT EXISTS QuestionBank (
    bank_question_id INT AUTO_INCREMENT PRIMARY KEY,
    question_text TEXT NOT NULL,
    difficulty ENUM('easy', 'medium', 'hard') NOT NULL,
    topic VARCHAR(255),
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    created_by INT,
    FOREIGN KEY (created_by) REFERENCES Users(user_id)
);

-- Tạo bảng BankAnswers (đáp án cho câu hỏi trong ngân hàng)
CREATE TABLE IF NOT EXISTS BankAnswers (
    bank_answer_id INT AUTO_INCREMENT PRIMARY KEY,
    bank_question_id INT,
    answer_text TEXT NOT NULL,
    is_correct BOOLEAN NOT NULL,
    FOREIGN KEY (bank_question_id) REFERENCES QuestionBank(bank_question_id)
);

-- Tạo bảng Logs
CREATE TABLE IF NOT EXISTS Logs (
    log_id INT AUTO_INCREMENT PRIMARY KEY,
    user_id INT,
    action TEXT NOT NULL,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES Users(user_id)
);

-- Dữ liệu mẫu cho bảng Users
INSERT INTO Users (username, password, email, role) VALUES
('admin', 'admin_password', 'admin@example.com', 'admin'),
('teacher1', 'teacher_password', 'teacher1@example.com', 'teacher'),
('student1', 'student_password', 'student1@example.com', 'student');

-- Dữ liệu mẫu cho bảng Quizzes
INSERT INTO Quizzes (title, description, question_count, time_limit, creator_id, status, exam_type, exam_start_time, exam_end_time) VALUES
('Câu hỏi về C++', 'Đề thi trắc nghiệm về C++', 10, 600, 1, 'not_started', 'normal', NULL, NULL),
('Đề thi Toán học', 'Đề thi về toán cơ bản', 5, 300, 2, 'not_started', 'scheduled', '2025-01-20 09:00:00', '2025-01-20 12:00:00');

-- Dữ liệu mẫu cho bảng Questions
INSERT INTO Questions (quiz_id, question_text, difficulty, topic) VALUES
(1, '2 + 2 = ?', 'easy', 'Math'),
(1, 'C++ là ngôn ngữ lập trình nào?', 'medium', 'Programming'),
(2, 'Công thức diện tích hình vuông là gì?', 'easy', 'Math');

-- Dữ liệu mẫu cho bảng Answers
INSERT INTO Answers (question_id, answer_text, is_correct) VALUES
(1, '3', 0),
(1, '4', 1),
(1, '5', 0),
(1, '6', 0),
(2, 'Hướng đối tượng', 1),
(2, 'Hướng thủ tục', 0),
(2, 'Hướng chức năng', 0),
(2, 'Hướng sự kiện', 0),
(3, 'S = a^2', 1),
(3, 'S = a * b', 0),
(3, 'S = 2a', 0),
(3, 'S = a + b', 0);

-- Dữ liệu mẫu cho bảng Exams
INSERT INTO Exams (quiz_id, user_id, start_time, end_time, score, status) VALUES
(1, 3, '2025-11-01 09:00:00', '2025-11-01 09:30:00', 0, 'in_progress');

-- Dữ liệu mẫu cho bảng Exam_Answers
INSERT INTO Exam_Answers (exam_id, question_id, answer_id, chosen_answer) VALUES
(1, 1, 2, '4'),
(1, 2, 1, 'Hướng đối tượng');

-- Dữ liệu mẫu cho bảng QuestionBank
INSERT INTO QuestionBank (question_text, difficulty, topic, created_by) VALUES
('C++ được phát triển bởi ai?', 'easy', 'C++ Basics', 2),
('Từ khóa nào dùng để kế thừa trong C++?', 'medium', 'C++ OOP', 2),
('Toán tử nào dùng để giải phóng bộ nhớ động trong C++?', 'medium', 'C++ Memory', 2),
('Container nào trong STL cho phép truy cập ngẫu nhiên?', 'hard', 'C++ STL', 2),
('Kích thước của con trỏ trong hệ thống 64-bit là bao nhiêu?', 'hard', 'C++ Pointers', 2),
('Giá trị của sin(90°) là?', 'easy', 'Math', 2),
('Nghiệm của phương trình x² - 5x + 6 = 0 là?', 'medium', 'Math', 2),
('Diện tích hình tròn có bán kính r được tính bằng công thức?', 'easy', 'Math', 2),
('Đạo hàm của x² là gì?', 'medium', 'Math', 2),
('Giới hạn của sin(x)/x khi x tiến về 0 là?', 'hard', 'Math', 2);

-- Dữ liệu mẫu cho bảng BankAnswers
-- Câu hỏi 1: C++ được phát triển bởi ai?
INSERT INTO BankAnswers (bank_question_id, answer_text, is_correct) VALUES
(1, 'Bjarne Stroustrup', 1),
(1, 'Dennis Ritchie', 0),
(1, 'James Gosling', 0),
(1, 'Guido van Rossum', 0);

-- Câu hỏi 2: Từ khóa nào dùng để kế thừa trong C++?
INSERT INTO BankAnswers (bank_question_id, answer_text, is_correct) VALUES
(2, 'extends', 0),
(2, ':', 1),
(2, 'inherits', 0),
(2, 'super', 0);

-- Câu hỏi 3: Toán tử nào dùng để giải phóng bộ nhớ động trong C++?
INSERT INTO BankAnswers (bank_question_id, answer_text, is_correct) VALUES
(3, 'free()', 0),
(3, 'delete', 1),
(3, 'remove()', 0),
(3, 'clear()', 0);

-- Câu hỏi 4: Container nào trong STL cho phép truy cập ngẫu nhiên?
INSERT INTO BankAnswers (bank_question_id, answer_text, is_correct) VALUES
(4, 'list', 0),
(4, 'vector', 1),
(4, 'queue', 0),
(4, 'stack', 0);

-- Câu hỏi 5: Kích thước của con trỏ trong hệ thống 64-bit
INSERT INTO BankAnswers (bank_question_id, answer_text, is_correct) VALUES
(5, '4 bytes', 0),
(5, '8 bytes', 1),
(5, '16 bytes', 0),
(5, 'Tùy thuộc vào kiểu dữ liệu', 0);

-- Câu hỏi 6: Giá trị của sin(90°)
INSERT INTO BankAnswers (bank_question_id, answer_text, is_correct) VALUES
(6, '0', 0),
(6, '1', 1),
(6, '-1', 0),
(6, '0.5', 0);

-- Câu hỏi 7: Nghiệm của phương trình x² - 5x + 6 = 0
INSERT INTO BankAnswers (bank_question_id, answer_text, is_correct) VALUES
(7, 'x = 2, x = 3', 1),
(7, 'x = 1, x = 6', 0),
(7, 'x = -2, x = -3', 0),
(7, 'x = 0, x = 5', 0);

-- Câu hỏi 8: Diện tích hình tròn
INSERT INTO BankAnswers (bank_question_id, answer_text, is_correct) VALUES
(8, 'πr²', 1),
(8, '2πr', 0),
(8, 'πr', 0),
(8, '2πr²', 0);

-- Câu hỏi 9: Đạo hàm của x²
INSERT INTO BankAnswers (bank_question_id, answer_text, is_correct) VALUES
(9, 'x', 0),
(9, '2x', 1),
(9, 'x²', 0),
(9, '2x²', 0);

-- Câu hỏi 10: Giới hạn của sin(x)/x khi x tiến về 0
INSERT INTO BankAnswers (bank_question_id, answer_text, is_correct) VALUES
(10, '0', 0),
(10, '1', 1),
(10, '∞', 0),
(10, 'Không tồn tại', 0);

-- Dữ liệu mẫu cho bảng Logs
INSERT INTO Logs (user_id, action) VALUES
(1, 'login'),
(3, 'start_exam'),
(3, 'submit_exam');

