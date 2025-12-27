#include "../../../include/gui/student/gui_exam.h"
#include "../../../include/client.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/select.h>
#include <unistd.h>

// Question data structure
struct Question
{
    int id;
    std::string text;
    std::string optA, optB, optC, optD;
    std::string selectedAnswer; // "A", "B", "C", "D" or ""
    bool answered;
    
    // Constructor để khởi tạo mặc định
    Question() : id(0), text(""), optA(""), optB(""), optC(""), optD(""), selectedAnswer(""), answered(false) {}
};

// Exam window data
struct ExamData
{
    GtkWidget *window;
    GtkWidget *question_label;
    GtkWidget *radio_a, *radio_b, *radio_c, *radio_d;
    GtkWidget *progress_label;
    GtkWidget *timer_label;
    GtkWidget *prev_btn, *next_btn, *submit_btn;
    GtkWidget *goto_entry;

    int sock;
    int currentIndex;
    int totalQuestions;
    int remainingSeconds;
    guint timer_id;
    std::vector<Question> questions;
    ExamResult *result;
    bool waitingForServer;
};

// Forward declarations
static void request_question(ExamData *data, int index);
static void update_display(ExamData *data);
static gboolean check_server_response(gpointer user_data);
static void on_answer_selected(GtkToggleButton *togglebutton, gpointer user_data);
static gboolean update_timer(gpointer user_data);

// Timer callback - updates countdown
static gboolean update_timer(gpointer user_data)
{
    ExamData *exam = (ExamData *)user_data;

    if (exam->remainingSeconds <= 0)
    {
        // Time's up - auto submit
        gtk_label_set_markup(GTK_LABEL(exam->timer_label),
                             "<span foreground='red' size='large' weight='bold'>⏰ Hết giờ!</span>");

        // Submit exam
        std::cout << "[Timer] Time expired - auto submitting exam\n";
        sendLine(exam->sock, "SUBMIT_EXAM_NOW");

        // Stop timer
        exam->timer_id = 0;
        return FALSE; // Stop timer
    }

    exam->remainingSeconds--;

    // Format: MM:SS
    int minutes = exam->remainingSeconds / 60;
    int seconds = exam->remainingSeconds % 60;

    char time_str[64];
    snprintf(time_str, sizeof(time_str), "⏱️ Thời gian còn lại: %02d:%02d", minutes, seconds);

    // Change color when time is running out
    if (exam->remainingSeconds < 60)
    {
        // Less than 1 minute - red
        char markup[128];
        snprintf(markup, sizeof(markup),
                 "<span foreground='red' size='large' weight='bold'>⏱️ %02d:%02d</span>",
                 minutes, seconds);
        gtk_label_set_markup(GTK_LABEL(exam->timer_label), markup);
    }
    else if (exam->remainingSeconds < 300)
    {
        // Less than 5 minutes - orange
        char markup[128];
        snprintf(markup, sizeof(markup),
                 "<span foreground='orange' size='large' weight='bold'>⏱️ %02d:%02d</span>",
                 minutes, seconds);
        gtk_label_set_markup(GTK_LABEL(exam->timer_label), markup);
    }
    else
    {
        // Normal - green
        char markup[128];
        snprintf(markup, sizeof(markup),
                 "<span foreground='green' size='large' weight='bold'>⏱️ %02d:%02d</span>",
                 minutes, seconds);
        gtk_label_set_markup(GTK_LABEL(exam->timer_label), markup);
    }

    return TRUE; // Continue timer
}

// Callback: Answer selected (save to local state)
static void on_answer_selected(GtkToggleButton *togglebutton, gpointer user_data)
{
    ExamData *exam = (ExamData *)user_data;

    // Only process if this radio is being activated (not deactivated)
    if (!gtk_toggle_button_get_active(togglebutton))
    {
        return;
    }

    int idx = exam->currentIndex;

    // Determine which answer was selected
    if (GTK_WIDGET(togglebutton) == exam->radio_a)
    {
        exam->questions[idx].selectedAnswer = "A";
    }
    else if (GTK_WIDGET(togglebutton) == exam->radio_b)
    {
        exam->questions[idx].selectedAnswer = "B";
    }
    else if (GTK_WIDGET(togglebutton) == exam->radio_c)
    {
        exam->questions[idx].selectedAnswer = "C";
    }
    else if (GTK_WIDGET(togglebutton) == exam->radio_d)
    {
        exam->questions[idx].selectedAnswer = "D";
    }

    exam->questions[idx].answered = true;

    // Update progress display
    int answered = 0;
    for (const auto &q : exam->questions)
    {
        if (q.answered)
            answered++;
    }

    std::stringstream ss;
    ss << "Đã trả lời: " << answered << "/" << exam->totalQuestions;
    gtk_label_set_text(GTK_LABEL(exam->progress_label), ss.str().c_str());

    std::cout << "[Answer " << exam->questions[idx].selectedAnswer << " selected for question " << (idx + 1) << "]\n";
}

// Callback: Previous button
static void on_prev_clicked(GtkWidget *widget, gpointer data)
{
    (void)widget;
    ExamData *exam = (ExamData *)data;

    if (exam->currentIndex > 0 && !exam->waitingForServer)
    {
        // Save current answer to server
        int idx = exam->currentIndex;
        if (!exam->questions[idx].selectedAnswer.empty())
        {
            std::string msg = "SAVE_ANSWER|" + std::to_string(exam->questions[idx].id) +
                              "|" + exam->questions[idx].selectedAnswer;
            sendLine(exam->sock, msg);
        }

        request_question(exam, exam->currentIndex - 1);
    }
}

// Callback: Next button
static void on_next_clicked(GtkWidget *widget, gpointer data)
{
    (void)widget;
    ExamData *exam = (ExamData *)data;

    if (exam->currentIndex < exam->totalQuestions - 1 && !exam->waitingForServer)
    {
        // Save current answer to server
        int idx = exam->currentIndex;
        if (!exam->questions[idx].selectedAnswer.empty())
        {
            std::string msg = "SAVE_ANSWER|" + std::to_string(exam->questions[idx].id) +
                              "|" + exam->questions[idx].selectedAnswer;
            sendLine(exam->sock, msg);
        }

        request_question(exam, exam->currentIndex + 1);
    }
}

// Callback: Go to question
static void on_goto_clicked(GtkWidget *widget, gpointer data)
{
    (void)widget;
    ExamData *exam = (ExamData *)data;

    const char *text = gtk_entry_get_text(GTK_ENTRY(exam->goto_entry));
    int target = atoi(text);

    if (target >= 1 && target <= exam->totalQuestions && !exam->waitingForServer)
    {
        // Save current answer to server
        int idx = exam->currentIndex;
        if (!exam->questions[idx].selectedAnswer.empty())
        {
            std::string msg = "SAVE_ANSWER|" + std::to_string(exam->questions[idx].id) +
                              "|" + exam->questions[idx].selectedAnswer;
            sendLine(exam->sock, msg);
        }

        request_question(exam, target - 1);
        gtk_entry_set_text(GTK_ENTRY(exam->goto_entry), "");
    }
}

// Callback: Submit exam
static void on_submit_clicked(GtkWidget *widget, gpointer data)
{
    (void)widget;
    ExamData *exam = (ExamData *)data;

    // Save current answer to server first
    int idx = exam->currentIndex;
    if (!exam->questions[idx].selectedAnswer.empty())
    {
        std::string msg = "SAVE_ANSWER|" + std::to_string(exam->questions[idx].id) +
                          "|" + exam->questions[idx].selectedAnswer;
        sendLine(exam->sock, msg);
    }

    // Count answered questions
    int answeredCount = 0;
    for (const auto &q : exam->questions)
    {
        if (q.answered)
            answeredCount++;
    }

    // Confirmation dialog
    std::stringstream ss;
    ss << "Bạn đã trả lời " << answeredCount << "/" << exam->totalQuestions << " câu hỏi.\n";
    if (answeredCount < exam->totalQuestions)
    {
        ss << "Còn " << (exam->totalQuestions - answeredCount) << " câu chưa trả lời!\n\n";
    }
    ss << "Xác nhận nộp bài?";

    GtkWidget *dialog = gtk_message_dialog_new(
        GTK_WINDOW(exam->window),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION,
        GTK_BUTTONS_YES_NO,
        "%s", ss.str().c_str());

    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    if (response == GTK_RESPONSE_YES)
    {
        sendLine(exam->sock, "SUBMIT_EXAM_NOW");
        exam->waitingForServer = true;
    }
}

// Request question from server
static void request_question(ExamData *data, int index)
{
    data->waitingForServer = true;
    data->currentIndex = index;

    std::string msg = "GO_TO_QUESTION|" + std::to_string(index);
    sendLine(data->sock, msg);

    std::cout << "[DEBUG] Requesting question " << (index + 1) << std::endl;
}

// Update GUI display
static void update_display(ExamData *data)
{
    // Kiểm tra bounds
    if (data->currentIndex < 0 || data->currentIndex >= (int)data->questions.size())
    {
        std::cerr << "[ERROR] Invalid currentIndex: " << data->currentIndex << std::endl;
        return;
    }
    
    Question &q = data->questions[data->currentIndex];
    
    // Kiểm tra nếu câu hỏi chưa được load
    if (q.text.empty())
    {
        std::cerr << "[WARNING] Question " << data->currentIndex << " chưa có dữ liệu\n";
        return;
    }

    // Update question text
    std::stringstream ss;
    ss << "Câu " << (data->currentIndex + 1) << " / " << data->totalQuestions << "\n\n";
    ss << q.text;
    gtk_label_set_text(GTK_LABEL(data->question_label), ss.str().c_str());

    // Update options
    gtk_button_set_label(GTK_BUTTON(data->radio_a), ("A. " + q.optA).c_str());
    gtk_button_set_label(GTK_BUTTON(data->radio_b), ("B. " + q.optB).c_str());
    gtk_button_set_label(GTK_BUTTON(data->radio_c), ("C. " + q.optC).c_str());
    gtk_button_set_label(GTK_BUTTON(data->radio_d), ("D. " + q.optD).c_str());

    // Block signals to prevent triggering on_answer_selected during programmatic update
    g_signal_handlers_block_by_func(data->radio_a, (gpointer)on_answer_selected, data);
    g_signal_handlers_block_by_func(data->radio_b, (gpointer)on_answer_selected, data);
    g_signal_handlers_block_by_func(data->radio_c, (gpointer)on_answer_selected, data);
    g_signal_handlers_block_by_func(data->radio_d, (gpointer)on_answer_selected, data);

    // Select previous answer
    if (q.selectedAnswer == "A")
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->radio_a), TRUE);
    else if (q.selectedAnswer == "B")
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->radio_b), TRUE);
    else if (q.selectedAnswer == "C")
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->radio_c), TRUE);
    else if (q.selectedAnswer == "D")
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->radio_d), TRUE);
    else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->radio_a), FALSE);

    // Unblock signals
    g_signal_handlers_unblock_by_func(data->radio_a, (gpointer)on_answer_selected, data);
    g_signal_handlers_unblock_by_func(data->radio_b, (gpointer)on_answer_selected, data);
    g_signal_handlers_unblock_by_func(data->radio_c, (gpointer)on_answer_selected, data);
    g_signal_handlers_unblock_by_func(data->radio_d, (gpointer)on_answer_selected, data);

    // Update progress
    int answered = 0;
    for (const auto &question : data->questions)
    {
        if (question.answered)
            answered++;
    }

    ss.str("");
    ss << "Đã trả lời: " << answered << "/" << data->totalQuestions;
    gtk_label_set_text(GTK_LABEL(data->progress_label), ss.str().c_str());

    // Enable/disable navigation buttons
    gtk_widget_set_sensitive(data->prev_btn, data->currentIndex > 0);
    gtk_widget_set_sensitive(data->next_btn, data->currentIndex < data->totalQuestions - 1);
}

// Check for server responses (called periodically)
static gboolean check_server_response(gpointer user_data)
{
    ExamData *data = (ExamData *)user_data;

    if (!hasData(data->sock, 0))
    {
        return TRUE; // Continue polling
    }

    std::string msg = recvLine(data->sock);
    if (msg.empty())
    {
        std::cerr << "[Connection lost]\n";
        gtk_main_quit();
        return FALSE;
    }

    std::cout << "[DEBUG] Server: " << msg << std::endl;

    auto parts = split(msg, '|');
    if (parts.empty())
        return TRUE;

    std::string cmd = parts[0];

    if (cmd == "QUESTION")
    {
        // Format: QUESTION|qId|text|A|B|C|D|currentIndex|totalQuestions|previousAnswer
        std::cout << "[DEBUG] QUESTION parts.size() = " << parts.size() << std::endl;
        if (parts.size() < 7)
        {
            std::cerr << "[ERROR] QUESTION response thiếu dữ liệu\n";
            return TRUE;
        }

        int qid = std::stoi(parts[1]);
        std::string text = parts[2];
        std::string optA = parts[3];
        std::string optB = parts[4];
        std::string optC = parts[5];
        std::string optD = parts[6];

        int index = data->currentIndex;
        if (parts.size() >= 8)
        {
            index = std::stoi(parts[7]);
        }

        std::string prevAnswer = "";
        if (parts.size() >= 10 && !parts[9].empty())
        {
            prevAnswer = parts[9];
        }

        // Update question data
        if (index >= 0 && index < data->totalQuestions)
        {
            data->questions[index].id = qid;
            data->questions[index].text = text;
            data->questions[index].optA = optA;
            data->questions[index].optB = optB;
            data->questions[index].optC = optC;
            data->questions[index].optD = optD;

            // Set selected answer
            if (!prevAnswer.empty())
            {
                if (prevAnswer == optA)
                    data->questions[index].selectedAnswer = "A";
                else if (prevAnswer == optB)
                    data->questions[index].selectedAnswer = "B";
                else if (prevAnswer == optC)
                    data->questions[index].selectedAnswer = "C";
                else if (prevAnswer == optD)
                    data->questions[index].selectedAnswer = "D";
                data->questions[index].answered = true;
            }

            data->currentIndex = index;
            data->waitingForServer = false;
            update_display(data);
        }
    }
    else if (cmd == "END_EXAM")
    {
        // Format: END_EXAM|score|correctAnswers
        data->result->completed = true;
        if (parts.size() >= 2)
        {
            data->result->score = std::stoi(parts[1]);
        }
        if (parts.size() >= 3)
        {
            data->result->correctAnswers = std::stoi(parts[2]);
        }
        data->result->totalQuestions = data->totalQuestions;

        // Show result dialog
        std::stringstream ss;
        ss << "Điểm: " << data->result->score << "\n";
        ss << "Số câu đúng: " << data->result->correctAnswers << "/" << data->totalQuestions;

        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(data->window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO,
            GTK_BUTTONS_OK,
            "Kết quả thi\n\n%s", ss.str().c_str());
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);

        gtk_main_quit();
    }
    else if (cmd == "SAVE_ANSWER_OK")
    {
        std::cout << "[Answer saved]\n";
    }

    return TRUE;
}

// Main function to show exam window
ExamResult showExamWindow(int sock, int quizId, const std::string &quizTitle)
{
    ExamResult result = {false, 0, 0, 0};

    // Request to join room
    sendLine(sock, "JOIN_ROOM|" + std::to_string(quizId));
    std::string response = recvLine(sock);

    std::cout << "[DEBUG] Join response: " << response << std::endl;

    auto parts = split(response, '|');
    if (parts.empty() || parts[0] != "TEST_STARTED")
    {
        GtkWidget *dialog = gtk_message_dialog_new(
            NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
            "Không thể vào phòng thi!\n%s", response.c_str());
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return result;
    }

    // TEST_STARTED|timeLimit|examType|totalQuestions
    int timeLimit = 600; // Default 10 minutes
    int totalQuestions = 0;
    if (parts.size() >= 2)
    {
        timeLimit = std::stoi(parts[1]);
    }
    if (parts.size() >= 4)
    {
        totalQuestions = std::stoi(parts[3]);
    }

    std::cout << "[Exam] Time limit: " << timeLimit << " seconds\n";

    // Create window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), ("Bài thi: " + quizTitle).c_str());
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    // Allocate exam data on heap (important for callbacks!)
    ExamData *data = new ExamData();
    data->window = window;
    data->sock = sock;
    data->currentIndex = 0;
    data->totalQuestions = totalQuestions;
    data->remainingSeconds = timeLimit;
    data->timer_id = 0;
    data->questions.resize(totalQuestions);
    data->result = &result;
    data->waitingForServer = false;

    // Main container
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);

    // Title
    GtkWidget *title_label = gtk_label_new(NULL);
    std::string markup = "<span size='large' weight='bold'>" + quizTitle + "</span>";
    gtk_label_set_markup(GTK_LABEL(title_label), markup.c_str());
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 5);

    // Timer label
    data->timer_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(data->timer_label),
                         "<span foreground='green' size='large' weight='bold'>⏱️ Loading...</span>");
    gtk_box_pack_start(GTK_BOX(vbox), data->timer_label, FALSE, FALSE, 5);

    // Progress label
    data->progress_label = gtk_label_new("Đang tải...");
    gtk_box_pack_start(GTK_BOX(vbox), data->progress_label, FALSE, FALSE, 5);

    // Separator
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);

    // Question label frame
    GtkWidget *question_frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(question_frame), GTK_SHADOW_IN);

    data->question_label = gtk_label_new("Đang tải câu hỏi...");
    gtk_label_set_line_wrap(GTK_LABEL(data->question_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(data->question_label), 0);
    gtk_label_set_yalign(GTK_LABEL(data->question_label), 0);
    gtk_widget_set_margin_start(data->question_label, 15);
    gtk_widget_set_margin_end(data->question_label, 15);
    gtk_widget_set_margin_top(data->question_label, 15);
    gtk_widget_set_margin_bottom(data->question_label, 15);

    gtk_container_add(GTK_CONTAINER(question_frame), data->question_label);
    gtk_box_pack_start(GTK_BOX(vbox), question_frame, FALSE, FALSE, 10);

    // Radio buttons for answers
    data->radio_a = gtk_radio_button_new_with_label(NULL, "A. ");
    data->radio_b = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(data->radio_a), "B. ");
    data->radio_c = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(data->radio_a), "C. ");
    data->radio_d = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(data->radio_a), "D. ");

    // Connect signals for answer selection
    g_signal_connect(data->radio_a, "toggled", G_CALLBACK(on_answer_selected), data);
    g_signal_connect(data->radio_b, "toggled", G_CALLBACK(on_answer_selected), data);
    g_signal_connect(data->radio_c, "toggled", G_CALLBACK(on_answer_selected), data);
    g_signal_connect(data->radio_d, "toggled", G_CALLBACK(on_answer_selected), data);

    gtk_box_pack_start(GTK_BOX(vbox), data->radio_a, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), data->radio_b, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), data->radio_c, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), data->radio_d, FALSE, FALSE, 5);

    // Navigation controls
    GtkWidget *nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(nav_box, GTK_ALIGN_CENTER);

    data->prev_btn = gtk_button_new_with_label("◀ Câu trước");
    gtk_widget_set_size_request(data->prev_btn, 120, 40);
    g_signal_connect(data->prev_btn, "clicked", G_CALLBACK(on_prev_clicked), data);
    gtk_box_pack_start(GTK_BOX(nav_box), data->prev_btn, FALSE, FALSE, 5);

    data->next_btn = gtk_button_new_with_label("Câu sau ▶");
    gtk_widget_set_size_request(data->next_btn, 120, 40);
    g_signal_connect(data->next_btn, "clicked", G_CALLBACK(on_next_clicked), data);
    gtk_box_pack_start(GTK_BOX(nav_box), data->next_btn, FALSE, FALSE, 5);

    GtkWidget *goto_label = gtk_label_new("Đến câu:");
    gtk_box_pack_start(GTK_BOX(nav_box), goto_label, FALSE, FALSE, 5);

    data->goto_entry = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(data->goto_entry), 5);
    gtk_box_pack_start(GTK_BOX(nav_box), data->goto_entry, FALSE, FALSE, 5);

    GtkWidget *goto_btn = gtk_button_new_with_label("Chuyển");
    g_signal_connect(goto_btn, "clicked", G_CALLBACK(on_goto_clicked), data);
    gtk_box_pack_start(GTK_BOX(nav_box), goto_btn, FALSE, FALSE, 5);

    data->submit_btn = gtk_button_new_with_label("✓ NỘP BÀI");
    gtk_widget_set_size_request(data->submit_btn, 120, 40);
    g_signal_connect(data->submit_btn, "clicked", G_CALLBACK(on_submit_clicked), data);
    gtk_box_pack_start(GTK_BOX(nav_box), data->submit_btn, FALSE, FALSE, 15);

    gtk_box_pack_start(GTK_BOX(vbox), nav_box, FALSE, FALSE, 10);

    gtk_container_add(GTK_CONTAINER(window), vbox);

    // Destroy window handler - cleanup data when window closes
    g_signal_connect_swapped(window, "destroy", G_CALLBACK(g_free), data);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Start polling for server messages
    g_timeout_add(100, check_server_response, data);

    // Start countdown timer (1 second interval)
    data->timer_id = g_timeout_add(1000, update_timer, data);

    // Request first question
    request_question(data, 0);

    gtk_widget_show_all(window);
    gtk_main();

    // Stop timer if still running
    if (data->timer_id > 0)
    {
        g_source_remove(data->timer_id);
    }

    // Cleanup
    gtk_widget_destroy(window);
    while (gtk_events_pending())
        gtk_main_iteration();

    return result;
}
