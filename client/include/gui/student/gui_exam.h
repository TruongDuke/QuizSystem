#ifndef GUI_EXAM_H
#define GUI_EXAM_H

#include <gtk/gtk.h>
#include <string>

// Kết quả sau khi thi xong
struct ExamResult {
    bool completed;
    int score;
    int correctAnswers;
    int totalQuestions;
};

// Hiển thị giao diện làm bài thi
ExamResult showExamWindow(int sock, int quizId, const std::string& quizTitle);

#endif // GUI_EXAM_H
