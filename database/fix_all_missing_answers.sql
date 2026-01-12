-- Script tự động sửa tất cả câu hỏi thiếu đáp án
-- Match câu hỏi trong Questions với QuestionBank dựa vào question_text
-- Sử dụng: mysql -u root -p quizDB < database/fix_all_missing_answers.sql

USE quizDB;

-- ============================================
-- BƯỚC 1: Kiểm tra câu hỏi thiếu đáp án
-- ============================================
SELECT '=== QUESTIONS MISSING ANSWERS (BEFORE FIX) ===' as info;

SELECT 
    q.question_id,
    q.question_text,
    q.quiz_id,
    q.difficulty,
    q.topic
FROM Questions q
LEFT JOIN Answers a ON q.question_id = a.question_id
WHERE a.answer_id IS NULL
ORDER BY q.question_id;

-- ============================================
-- BƯỚC 2: Copy đáp án từ QuestionBank sang Answers
-- Match theo question_text (chính xác)
-- ============================================
SELECT '=== FIXING MISSING ANSWERS ===' as info;

-- Copy đáp án cho các câu hỏi thiếu đáp án
-- Match với QuestionBank dựa vào question_text
INSERT INTO Answers (question_id, answer_text, is_correct)
SELECT 
    q.question_id,
    ba.answer_text,
    ba.is_correct
FROM Questions q
INNER JOIN QuestionBank qb ON q.question_text = qb.question_text
INNER JOIN BankAnswers ba ON qb.bank_question_id = ba.bank_question_id
WHERE q.question_id NOT IN (
    SELECT DISTINCT question_id 
    FROM Answers 
    WHERE question_id IS NOT NULL
)
ORDER BY q.question_id, ba.bank_answer_id;

-- Hiển thị số lượng đáp án đã được thêm
SELECT 
    CONCAT('Added answers for questions') as result;

-- ============================================
-- BƯỚC 3: Kiểm tra kết quả
-- ============================================
SELECT '=== VERIFICATION (AFTER FIX) ===' as info;

-- Kiểm tra xem còn câu hỏi nào thiếu đáp án không
SELECT 
    q.question_id,
    q.question_text,
    q.quiz_id,
    COUNT(a.answer_id) as answer_count
FROM Questions q
LEFT JOIN Answers a ON q.question_id = a.question_id
GROUP BY q.question_id, q.question_text, q.quiz_id
HAVING answer_count = 0 OR answer_count IS NULL
ORDER BY q.question_id;

-- ============================================
-- BƯỚC 4: Tổng kết
-- ============================================
SELECT '=== SUMMARY ===' as info;

SELECT 
    COUNT(DISTINCT q.question_id) as total_questions,
    COUNT(DISTINCT CASE WHEN a.answer_id IS NOT NULL THEN q.question_id END) as questions_with_answers,
    COUNT(DISTINCT CASE WHEN a.answer_id IS NULL THEN q.question_id END) as questions_missing_answers
FROM Questions q
LEFT JOIN Answers a ON q.question_id = a.question_id;

-- Hiển thị thông báo cuối cùng
SELECT 
    CASE 
        WHEN COUNT(DISTINCT CASE WHEN a.answer_id IS NULL THEN q.question_id END) = 0 
        THEN 'SUCCESS: All questions now have answers!'
        ELSE CONCAT('WARNING: ', 
                   COUNT(DISTINCT CASE WHEN a.answer_id IS NULL THEN q.question_id END), 
                   ' questions still missing answers. They may not be in QuestionBank.')
    END as final_status
FROM Questions q
LEFT JOIN Answers a ON q.question_id = a.question_id;

