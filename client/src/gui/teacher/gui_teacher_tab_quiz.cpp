#include "../../../include/gui/teacher/gui_teacher_tab_quiz.h"
#include "../../../include/client.h"
#include <iostream>
#include <sstream>

// Quiz tab data
struct QuizTabData {
    GtkWidget *tree_view;
    GtkListStore *list_store;
    int sock;
};

// Quiz info structure (for storing in tree model)
struct QuizInfo {
    int id;
    std::string title;
    int questions;
    int timeLimit;
    std::string status;
    std::string examType;
    std::string startTime;
    std::string endTime;
};

// Quiz columns
enum {
    QUIZ_COL_ID,
    QUIZ_COL_TITLE,
    QUIZ_COL_QUESTIONS,
    QUIZ_COL_TIME,
    QUIZ_COL_STATUS,
    QUIZ_COL_EXAM_TYPE,
    QUIZ_COL_START_TIME,
    QUIZ_COL_END_TIME,
    QUIZ_NUM_COLS
};

// Load quizzes from server
static void load_quizzes(QuizTabData *data) {
    sendLine(data->sock, "LIST_QUIZZES");
    
    // Non-blocking check
    if (!hasData(data->sock, 1)) {
        std::cerr << "[Quiz] No response from server\n";
        return;
    }
    
    std::string response = recvLine(data->sock);
    std::cout << "[Quiz] Server: " << response << std::endl;
    
    gtk_list_store_clear(data->list_store);
    
    // Parse: QUIZZES|1:Title(10Q,600s,status,type,start,end);2:...
    if (response.find("QUIZZES|") != 0) {
        return;
    }
    
    std::string quizData = response.substr(8);
    if (quizData.empty()) return;
    
    auto items = split(quizData, ';');
    for (const auto& item : items) {
        if (item.empty()) continue;
        
        // Parse: "1:Title(10Q,600s,status,type,start,end)"
        size_t colonPos = item.find(':');
        if (colonPos == std::string::npos) continue;
        
        std::string idStr = item.substr(0, colonPos);
        std::string rest = item.substr(colonPos + 1);
        
        size_t openParen = rest.find('(');
        if (openParen == std::string::npos) continue;
        
        std::string title = rest.substr(0, openParen);
        std::string details = rest.substr(openParen + 1, rest.length() - openParen - 2);
        
        // Parse details: "10Q,600s,status,type,start,end"
        auto detailParts = split(details, ',');
        if (detailParts.size() < 3) continue;
        
        std::string questionsStr = detailParts[0]; // "10Q"
        std::string timeStr = detailParts[1];      // "600s"
        std::string status = detailParts[2];       // "not_started"
        std::string examType = detailParts.size() > 3 ? detailParts[3] : "normal";
        std::string startTime = detailParts.size() > 4 ? detailParts[4] : "NULL";
        std::string endTime = detailParts.size() > 5 ? detailParts[5] : "NULL";
        
        // Remove 'Q' and 's'
        int questions = 0, timeLimit = 0;
        try {
            if (questionsStr.back() == 'Q') questionsStr.pop_back();
            if (timeStr.back() == 's') timeStr.pop_back();
            questions = std::stoi(questionsStr);
            timeLimit = std::stoi(timeStr);
        } catch (...) {}
        
        GtkTreeIter iter;
        gtk_list_store_append(data->list_store, &iter);
        gtk_list_store_set(data->list_store, &iter,
            QUIZ_COL_ID, std::stoi(idStr),
            QUIZ_COL_TITLE, title.c_str(),
            QUIZ_COL_QUESTIONS, questions,
            QUIZ_COL_TIME, timeLimit,
            QUIZ_COL_STATUS, status.c_str(),
            QUIZ_COL_EXAM_TYPE, examType.c_str(),
            QUIZ_COL_START_TIME, startTime.c_str(),
            QUIZ_COL_END_TIME, endTime.c_str(),
            -1);
    }
}

// Callback for showing/hiding schedule widgets in add quiz dialog
struct ScheduleWidgets {
    GtkWidget *label;
    GtkWidget *start_label;
    GtkWidget *delay_spin;
    GtkWidget *note_label;
};

static void on_schedule_toggle_add(GtkToggleButton *btn, gpointer data) {
    gboolean active = gtk_toggle_button_get_active(btn);
    ScheduleWidgets *widgets = (ScheduleWidgets*)data;
    gtk_widget_set_visible(widgets->label, active);
    gtk_widget_set_visible(widgets->start_label, active);
    gtk_widget_set_visible(widgets->delay_spin, active);
    gtk_widget_set_visible(widgets->note_label, active);
}

// Callback: Refresh button
static void on_quiz_refresh_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    load_quizzes((QuizTabData*)data);
}

// Callback: Add quiz button
static void on_quiz_add_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    QuizTabData *tabData = (QuizTabData*)data;
    
    // Create dialog
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Tạo bài thi mới",
        NULL,
        GTK_DIALOG_MODAL,
        "Hủy", GTK_RESPONSE_CANCEL,
        "Tạo", GTK_RESPONSE_OK,
        NULL
    );
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 15);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    
    // Title
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Tên bài thi:"), 0, 0, 1, 1);
    GtkWidget *title_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(title_entry), "VD: Kiểm tra C++");
    gtk_widget_set_hexpand(title_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), title_entry, 1, 0, 1, 1);
    
    // Description
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Mô tả:"), 0, 1, 1, 1);
    GtkWidget *desc_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(desc_entry), "VD: Bài kiểm tra 15 phút");
    gtk_widget_set_hexpand(desc_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), desc_entry, 1, 1, 1, 1);
    
    // Question count
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Số câu hỏi:"), 0, 2, 1, 1);
    GtkWidget *count_spin = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(count_spin), 10);
    gtk_grid_attach(GTK_GRID(grid), count_spin, 1, 2, 1, 1);
    
    // Time limit
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Thời gian (phút):"), 0, 3, 1, 1);
    GtkWidget *time_spin = gtk_spin_button_new_with_range(1, 180, 5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(time_spin), 10);
    gtk_grid_attach(GTK_GRID(grid), time_spin, 1, 3, 1, 1);
    
    // Separator
    gtk_grid_attach(GTK_GRID(grid), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), 0, 4, 2, 1);
    
    // Exam Type
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Loại bài thi:"), 0, 5, 1, 1);
    GtkWidget *type_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *radio_normal = gtk_radio_button_new_with_label(NULL, "Thường");
    GtkWidget *radio_scheduled = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(radio_normal), "Theo lịch");
    gtk_box_pack_start(GTK_BOX(type_box), radio_normal, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(type_box), radio_scheduled, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), type_box, 1, 5, 1, 1);
    
    // Schedule section (initially hidden)
    GtkWidget *schedule_label = gtk_label_new("Hẹn giờ thi:");
    gtk_widget_set_margin_top(schedule_label, 10);
    gtk_grid_attach(GTK_GRID(grid), schedule_label, 0, 6, 2, 1);
    
    // Delay start (seconds from now)
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("  Bắt đầu sau (giây):"), 0, 8, 1, 1);
    GtkWidget *delay_spin = gtk_spin_button_new_with_range(1, 86400, 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(delay_spin), 60);
    gtk_grid_attach(GTK_GRID(grid), delay_spin, 1, 7, 1, 1);
    
    GtkWidget *note_label = gtk_label_new("(Thi sẽ kết thúc sau thời gian làm bài)");
    gtk_widget_set_halign(note_label, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), note_label, 1, 8, 1, 1);
    
    // Initially hide schedule widgets
    gtk_widget_set_visible(schedule_label, FALSE);
    gtk_widget_set_visible(gtk_grid_get_child_at(GTK_GRID(grid), 0, 7), FALSE);
    gtk_widget_set_visible(delay_spin, FALSE);
    gtk_widget_set_visible(note_label, FALSE);
    
    // Connect radio button signals to show/hide schedule widgets
    ScheduleWidgets *schedule_widgets = new ScheduleWidgets{
        schedule_label,
        gtk_grid_get_child_at(GTK_GRID(grid), 0, 7),
        delay_spin,
        note_label
    };
    g_signal_connect(radio_scheduled, "toggled", G_CALLBACK(on_schedule_toggle_add), schedule_widgets);
    
    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_show_all(dialog);
    
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (response == GTK_RESPONSE_OK) {
        const char *title = gtk_entry_get_text(GTK_ENTRY(title_entry));
        const char *desc = gtk_entry_get_text(GTK_ENTRY(desc_entry));
        int count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(count_spin));
        int timeMinutes = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(time_spin));
        int timeSeconds = timeMinutes * 60;
        
        if (strlen(title) == 0) {
            gtk_widget_destroy(dialog);
            
            GtkWidget *err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Tên bài thi không được để trống!");
            gtk_dialog_run(GTK_DIALOG(err));
            gtk_widget_destroy(err);
            return;
        }
        
        // Check exam type
        gboolean is_scheduled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_scheduled));
        std::string examType = is_scheduled ? "scheduled" : "normal";
        
        std::stringstream ss;
        ss << "ADD_QUIZ|" << title << "|" << desc << "|" << count << "|" << timeSeconds;
        
        if (is_scheduled) {
            int delay_seconds = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(delay_spin));
            ss << "|" << examType << "|" << delay_seconds;
        }
        
        sendLine(tabData->sock, ss.str());
        
        if (hasData(tabData->sock, 2)) {
            std::string resp = recvLine(tabData->sock);
            std::cout << "[Quiz] Add response: " << resp << std::endl;
            
            if (resp.find("ADD_QUIZ_OK") == 0) {
                load_quizzes(tabData);
            } else {
                GtkWidget *err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Tạo bài thi thất bại:\n%s", resp.c_str());
                gtk_dialog_run(GTK_DIALOG(err));
                gtk_widget_destroy(err);
            }
        }
    }
    
    gtk_widget_destroy(dialog);
}

// Callback: Edit quiz button
static void on_quiz_edit_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    QuizTabData *tabData = (QuizTabData*)data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tabData->tree_view));
    GtkTreeIter iter;
    GtkTreeModel *model;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }
    
    int quiz_id, questions, time_limit;
    gchar *title, *status, *exam_type, *start_time, *end_time;
    gtk_tree_model_get(model, &iter, 
        QUIZ_COL_ID, &quiz_id,
        QUIZ_COL_TITLE, &title,
        QUIZ_COL_QUESTIONS, &questions,
        QUIZ_COL_TIME, &time_limit,
        QUIZ_COL_STATUS, &status,
        QUIZ_COL_EXAM_TYPE, &exam_type,
        QUIZ_COL_START_TIME, &start_time,
        QUIZ_COL_END_TIME, &end_time,
        -1);
    
    // Create edit dialog
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Sửa bài thi",
        NULL,
        GTK_DIALOG_MODAL,
        "Hủy", GTK_RESPONSE_CANCEL,
        "Lưu", GTK_RESPONSE_OK,
        NULL
    );
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 15);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    
    // ID (readonly)
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("ID:"), 0, 0, 1, 1);
    GtkWidget *id_label = gtk_label_new(std::to_string(quiz_id).c_str());
    gtk_grid_attach(GTK_GRID(grid), id_label, 1, 0, 1, 1);
    
    // Title
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Tên bài thi:"), 0, 1, 1, 1);
    GtkWidget *title_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(title_entry), title);
    gtk_widget_set_hexpand(title_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), title_entry, 1, 1, 1, 1);
    
    // Description
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Mô tả:"), 0, 2, 1, 1);
    GtkWidget *desc_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(desc_entry), "Nhập mô tả mới");
    gtk_widget_set_hexpand(desc_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), desc_entry, 1, 2, 1, 1);
    
    // Question count
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Số câu hỏi:"), 0, 3, 1, 1);
    GtkWidget *count_spin = gtk_spin_button_new_with_range(1, 100, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(count_spin), questions);
    gtk_grid_attach(GTK_GRID(grid), count_spin, 1, 3, 1, 1);
    
    // Time limit
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Thời gian (phút):"), 0, 4, 1, 1);
    GtkWidget *time_spin = gtk_spin_button_new_with_range(1, 180, 5);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(time_spin), time_limit / 60);
    gtk_grid_attach(GTK_GRID(grid), time_spin, 1, 4, 1, 1);
    
    // Separator
    gtk_grid_attach(GTK_GRID(grid), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), 0, 5, 2, 1);
    
    // Exam Type
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Loại bài thi:"), 0, 6, 1, 1);
    GtkWidget *type_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *radio_normal = gtk_radio_button_new_with_label(NULL, "Thường");
    GtkWidget *radio_scheduled = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(radio_normal), "Theo lịch");
    
    // Set current exam type
    bool is_scheduled = (std::string(exam_type) == "scheduled");
    if (is_scheduled) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_scheduled), TRUE);
    }
    
    gtk_box_pack_start(GTK_BOX(type_box), radio_normal, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(type_box), radio_scheduled, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), type_box, 1, 6, 1, 1);
    
    // Schedule section
    GtkWidget *schedule_label_edit = gtk_label_new("Hẹn giờ thi:");
    gtk_widget_set_margin_top(schedule_label_edit, 10);
    gtk_grid_attach(GTK_GRID(grid), schedule_label_edit, 0, 7, 2, 1);
    
    // Delay start (seconds from now)
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("  Bắt đầu sau (giây):"), 0, 8, 1, 1);
    GtkWidget *delay_spin_edit = gtk_spin_button_new_with_range(1, 86400, 10);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(delay_spin_edit), 60);
    gtk_grid_attach(GTK_GRID(grid), delay_spin_edit, 1, 8, 1, 1);
    
    GtkWidget *note_label_edit = gtk_label_new("(Thi sẽ kết thúc sau thời gian làm bài)");
    gtk_widget_set_halign(note_label_edit, GTK_ALIGN_START);
    gtk_grid_attach(GTK_GRID(grid), note_label_edit, 1, 9, 1, 1);
    
    // Show/hide schedule widgets based on type
    gtk_widget_set_visible(schedule_label_edit, is_scheduled);
    gtk_widget_set_visible(gtk_grid_get_child_at(GTK_GRID(grid), 0, 8), is_scheduled);
    gtk_widget_set_visible(delay_spin_edit, is_scheduled);
    gtk_widget_set_visible(note_label_edit, is_scheduled);
    
    // Connect radio button signals to show/hide schedule widgets
    ScheduleWidgets *schedule_widgets_edit = new ScheduleWidgets{
        schedule_label_edit,
        gtk_grid_get_child_at(GTK_GRID(grid), 0, 8),
        delay_spin_edit,
        note_label_edit
    };
    g_signal_connect(radio_scheduled, "toggled", G_CALLBACK(on_schedule_toggle_add), schedule_widgets_edit);
    
    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_show_all(dialog);
    
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (response == GTK_RESPONSE_OK) {
        const char *new_title = gtk_entry_get_text(GTK_ENTRY(title_entry));
        const char *new_desc = gtk_entry_get_text(GTK_ENTRY(desc_entry));
        int new_count = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(count_spin));
        int new_time_minutes = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(time_spin));
        int new_time_seconds = new_time_minutes * 60;
        
        if (strlen(new_title) == 0) {
            gtk_widget_destroy(dialog);
            
            GtkWidget *err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Tên bài thi không được để trống!");
            gtk_dialog_run(GTK_DIALOG(err));
            gtk_widget_destroy(err);
            
            g_free(title);
            g_free(status);
            g_free(exam_type);
            g_free(start_time);
            g_free(end_time);
            return;
        }
        
        // Check exam type
        gboolean is_scheduled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_scheduled));
        std::string examType = is_scheduled ? "scheduled" : "normal";
        
        std::stringstream ss;
        ss << "EDIT_QUIZ|" << quiz_id << "|" << new_title << "|" 
           << (strlen(new_desc) > 0 ? new_desc : "No description") << "|" 
           << new_count << "|" << new_time_seconds;
        
        if (is_scheduled) {
            int delay_seconds = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(delay_spin_edit));
            ss << "|" << examType << "|" << delay_seconds;
        } else {
            // Normal exam - send normal type
            ss << "|normal";
        }
        
        sendLine(tabData->sock, ss.str());
        
        if (hasData(tabData->sock, 2)) {
            std::string resp = recvLine(tabData->sock);
            std::cout << "[Quiz] Edit response: " << resp << std::endl;
            
            if (resp.find("EDIT_QUIZ_OK") == 0) {
                load_quizzes(tabData);
            } else {
                GtkWidget *err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Sửa bài thi thất bại:\n%s", resp.c_str());
                gtk_dialog_run(GTK_DIALOG(err));
                gtk_widget_destroy(err);
            }
        }
    }
    
    gtk_widget_destroy(dialog);
    g_free(title);
    g_free(status);
    g_free(exam_type);
    g_free(start_time);
    g_free(end_time);
}

// Callback: Delete quiz button
static void on_quiz_delete_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    QuizTabData *tabData = (QuizTabData*)data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tabData->tree_view));
    GtkTreeIter iter;
    GtkTreeModel *model;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }
    
    int quiz_id;
    gchar *title;
    gtk_tree_model_get(model, &iter, 
        QUIZ_COL_ID, &quiz_id,
        QUIZ_COL_TITLE, &title,
        -1);
    
    // Confirmation dialog
    GtkWidget *dialog = gtk_message_dialog_new(
        NULL, GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
        "Xác nhận xóa quiz:\n\n%s (ID: %d)?", title, quiz_id
    );
    
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    if (response == GTK_RESPONSE_YES) {
        std::string msg = "DELETE_QUIZ|" + std::to_string(quiz_id);
        sendLine(tabData->sock, msg);
        
        if (hasData(tabData->sock, 1)) {
            std::string resp = recvLine(tabData->sock);
            std::cout << "[Quiz] Delete response: " << resp << std::endl;
            
            if (resp.find("DELETE_QUIZ_OK") == 0) {
                load_quizzes(tabData); // Reload list
            }
        }
    }
    
    g_free(title);
}

// Callback: Change quiz status (Start/Finish)
static void on_quiz_status_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    QuizTabData *tabData = (QuizTabData*)data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tabData->tree_view));
    GtkTreeIter iter;
    GtkTreeModel *model;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }
    
    int quiz_id;
    gchar *title, *status, *exam_type;
    gtk_tree_model_get(model, &iter, 
        QUIZ_COL_ID, &quiz_id,
        QUIZ_COL_TITLE, &title,
        QUIZ_COL_STATUS, &status,
        QUIZ_COL_EXAM_TYPE, &exam_type,
        -1);
    
    // Determine new status
    std::string current_status(status);
    std::string new_status;
    std::string action_text;
    
    if (current_status == "not_started") {
        new_status = "in_progress";
        action_text = "Bắt đầu";
    } else if (current_status == "in_progress") {
        new_status = "finished";
        action_text = "Kết thúc";
    } else {
        GtkWidget *msg = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO, GTK_BUTTONS_OK, 
            "Quiz đã kết thúc, không thể thay đổi trạng thái!");
        gtk_dialog_run(GTK_DIALOG(msg));
        gtk_widget_destroy(msg);
        g_free(title);
        g_free(status);
        g_free(exam_type);
        return;
    }
    
    // Confirmation
    std::string msg_text = action_text + " quiz:\n\n" + std::string(title) + 
                          "\n\nTrạng thái: " + current_status + " → " + new_status;
    GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s", msg_text.c_str());
    
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    if (response == GTK_RESPONSE_YES) {
        // Send STATUS_QUIZ command to server
        std::string cmd = "STATUS_QUIZ|" + std::to_string(quiz_id) + "|" + new_status;
        sendLine(tabData->sock, cmd);
        
        if (hasData(tabData->sock, 1)) {
            std::string resp = recvLine(tabData->sock);
            std::cout << "[Quiz] Status change response: " << resp << std::endl;
            
            if (resp.find("STATUS_QUIZ_OK") == 0) {
                load_quizzes(tabData);
            } else {
                GtkWidget *err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, 
                    "Thay đổi trạng thái thất bại:\n%s", resp.c_str());
                gtk_dialog_run(GTK_DIALOG(err));
                gtk_widget_destroy(err);
            }
        }
    }
    
    g_free(title);
    g_free(status);
    g_free(exam_type);
}

// Public function: Create Quiz Management tab
GtkWidget* createQuizTab(int sock) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
    
    // Allocate tab data on heap
    QuizTabData *tabData = new QuizTabData();
    tabData->sock = sock;
    
    // Header with buttons
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>Danh sách bài thi</b>");
    gtk_box_pack_start(GTK_BOX(header), title, FALSE, FALSE, 0);
    
    GtkWidget *add_btn = gtk_button_new_with_label("➕ Tạo mới");
    gtk_widget_set_size_request(add_btn, 100, 35);
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_quiz_add_clicked), tabData);
    gtk_box_pack_end(GTK_BOX(header), add_btn, FALSE, FALSE, 0);
    
    GtkWidget *refresh_btn = gtk_button_new_with_label("🔄 Refresh");
    gtk_widget_set_size_request(refresh_btn, 100, 35);
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_quiz_refresh_clicked), tabData);
    gtk_box_pack_end(GTK_BOX(header), refresh_btn, FALSE, FALSE, 0);
    
    GtkWidget *status_btn = gtk_button_new_with_label("▶️ Start/Stop");
    gtk_widget_set_size_request(status_btn, 120, 35);
    g_signal_connect(status_btn, "clicked", G_CALLBACK(on_quiz_status_clicked), tabData);
    gtk_box_pack_end(GTK_BOX(header), status_btn, FALSE, FALSE, 0);
    
    GtkWidget *delete_btn = gtk_button_new_with_label("🗑️ Xóa");
    gtk_widget_set_size_request(delete_btn, 100, 35);
    g_signal_connect(delete_btn, "clicked", G_CALLBACK(on_quiz_delete_clicked), tabData);
    gtk_box_pack_end(GTK_BOX(header), delete_btn, FALSE, FALSE, 0);
    
    GtkWidget *edit_btn = gtk_button_new_with_label("✏️ Sửa");
    gtk_widget_set_size_request(edit_btn, 100, 35);
    g_signal_connect(edit_btn, "clicked", G_CALLBACK(on_quiz_edit_clicked), tabData);
    gtk_box_pack_end(GTK_BOX(header), edit_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);
    
    // TreeView
    tabData->list_store = gtk_list_store_new(QUIZ_NUM_COLS,
        G_TYPE_INT,      // ID
        G_TYPE_STRING,   // Title
        G_TYPE_INT,      // Questions count
        G_TYPE_INT,      // Time limit
        G_TYPE_STRING,   // Status
        G_TYPE_STRING,   // Exam type
        G_TYPE_STRING,   // Start time
        G_TYPE_STRING    // End time
    );
    
    tabData->tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tabData->list_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tabData->tree_view), TRUE);
    
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    
    // ID column
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("ID", renderer, "text", QUIZ_COL_ID, NULL);
    gtk_tree_view_column_set_min_width(column, 50);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), column);
    
    // Title column
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Tên bài thi", renderer, "text", QUIZ_COL_TITLE, NULL);
    gtk_tree_view_column_set_min_width(column, 250);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), column);
    
    // Questions count column
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Số câu", renderer, "text", QUIZ_COL_QUESTIONS, NULL);
    gtk_tree_view_column_set_min_width(column, 70);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), column);
    
    // Time limit column
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Thời gian (s)", renderer, "text", QUIZ_COL_TIME, NULL);
    gtk_tree_view_column_set_min_width(column, 90);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), column);
    
    // Status column
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Trạng thái", renderer, "text", QUIZ_COL_STATUS, NULL);
    gtk_tree_view_column_set_min_width(column, 100);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), column);
    
    // Exam type column
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Loại", renderer, "text", QUIZ_COL_EXAM_TYPE, NULL);
    gtk_tree_view_column_set_min_width(column, 80);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), column);
    
    // Start time column (hidden by default, can be shown if needed)
    // renderer = gtk_cell_renderer_text_new();
    // column = gtk_tree_view_column_new_with_attributes("Bắt đầu", renderer, "text", QUIZ_COL_START_TIME, NULL);
    // gtk_tree_view_column_set_min_width(column, 150);
    // gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), column);
    
    // Scrolled window
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), tabData->tree_view);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    
    // Load initial data
    load_quizzes(tabData);
    
    return vbox;
}
