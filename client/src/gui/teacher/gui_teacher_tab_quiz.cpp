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

// Quiz columns
enum {
    QUIZ_COL_ID,
    QUIZ_COL_TITLE,
    QUIZ_COL_QUESTIONS,
    QUIZ_COL_TIME,
    QUIZ_COL_STATUS,
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
    
    // Parse: QUIZZES|1:Title(10Q,600s,status);2:...
    if (response.find("QUIZZES|") != 0) {
        return;
    }
    
    std::string quizData = response.substr(8);
    if (quizData.empty()) return;
    
    auto items = split(quizData, ';');
    for (const auto& item : items) {
        if (item.empty()) continue;
        
        // Parse: "1:Title(10Q,600s,status)"
        size_t colonPos = item.find(':');
        if (colonPos == std::string::npos) continue;
        
        std::string idStr = item.substr(0, colonPos);
        std::string rest = item.substr(colonPos + 1);
        
        size_t openParen = rest.find('(');
        if (openParen == std::string::npos) continue;
        
        std::string title = rest.substr(0, openParen);
        std::string details = rest.substr(openParen + 1, rest.length() - openParen - 2);
        
        // Parse details: "10Q,600s,status"
        auto detailParts = split(details, ',');
        if (detailParts.size() < 3) continue;
        
        std::string questionsStr = detailParts[0]; // "10Q"
        std::string timeStr = detailParts[1];      // "600s"
        std::string status = detailParts[2];       // "not_started"
        
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
            -1);
    }
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
        
        // Send to server: ADD_QUIZ|title|desc|count|time
        std::stringstream ss;
        ss << "ADD_QUIZ|" << title << "|" << desc << "|" << count << "|" << timeSeconds;
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
    gchar *title, *status;
    gtk_tree_model_get(model, &iter, 
        QUIZ_COL_ID, &quiz_id,
        QUIZ_COL_TITLE, &title,
        QUIZ_COL_QUESTIONS, &questions,
        QUIZ_COL_TIME, &time_limit,
        QUIZ_COL_STATUS, &status,
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
            return;
        }
        
        // Send: EDIT_QUIZ|quizId|title|desc|count|time
        std::stringstream ss;
        ss << "EDIT_QUIZ|" << quiz_id << "|" << new_title << "|" 
           << (strlen(new_desc) > 0 ? new_desc : "No description") << "|" 
           << new_count << "|" << new_time_seconds;
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
        G_TYPE_STRING    // Status
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
    gtk_tree_view_column_set_min_width(column, 300);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), column);
    
    // Questions count column
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Số câu", renderer, "text", QUIZ_COL_QUESTIONS, NULL);
    gtk_tree_view_column_set_min_width(column, 80);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), column);
    
    // Time limit column
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Thời gian (s)", renderer, "text", QUIZ_COL_TIME, NULL);
    gtk_tree_view_column_set_min_width(column, 100);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), column);
    
    // Status column
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Trạng thái", renderer, "text", QUIZ_COL_STATUS, NULL);
    gtk_tree_view_column_set_min_width(column, 120);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), column);
    
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
