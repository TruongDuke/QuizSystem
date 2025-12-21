
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

-- Dữ liệu mẫu cho bảng Logs
INSERT INTO Logs (user_id, action) VALUES
(1, 'login'),
(3, 'start_exam'),
(3, 'submit_exam');
