#include "../../../include/gui/teacher/gui_teacher_tab_question.h"
#include "../../../include/client.h"
#include <iostream>
#include <sstream>

// Question tab data
struct QuestionTabData {
    GtkWidget *quiz_combo;
    GtkWidget *tree_view;
    GtkListStore *list_store;
    GtkListStore *quiz_store; // For combo
    int sock;
    int current_quiz_id;
};

// Question columns
enum {
    QSTN_COL_ID,
    QSTN_COL_CONTENT,
    QSTN_COL_OPT_A,
    QSTN_COL_OPT_B,
    QSTN_COL_OPT_C,
    QSTN_COL_OPT_D,
    QSTN_COL_CORRECT,
    QSTN_NUM_COLS
};

// Load questions for a specific quiz
static void load_questions_for_quiz(QuestionTabData *data, int quiz_id) {
    if (quiz_id <= 0) return;
    
    data->current_quiz_id = quiz_id;
    
    std::string msg = "LIST_QUESTIONS|" + std::to_string(quiz_id);
    sendLine(data->sock, msg);
    
    if (!hasData(data->sock, 1)) {
        std::cerr << "[Questions] No response\n";
        return;
    }
    
    std::string resp = recvLine(data->sock);
    std::cout << "[Questions] Server: " << resp << std::endl;
    
    gtk_list_store_clear(data->list_store);
    
    // Parse: QUESTIONS|quizId|id:text;id2:text2;...
    if (resp.find("QUESTIONS|") != 0) return;
    
    auto parts = split(resp, '|');
    if (parts.size() < 3) return; // QUESTIONS|quizId|...
    
    std::string questionData = parts[2];
    if (questionData.empty()) return;
    
    auto items = split(questionData, ';');
    for (const auto& item : items) {
        if (item.empty()) continue;
        
        size_t colonPos = item.find(':');
        if (colonPos == std::string::npos) continue;
        
        int qid = std::stoi(item.substr(0, colonPos));
        std::string content = item.substr(colonPos + 1);
        
        // Fetch full question details
        std::string detail_msg = "GET_ONE_QUESTION|" + std::to_string(qid);
        sendLine(data->sock, detail_msg);
        
        std::string opt_a = "", opt_b = "", opt_c = "", opt_d = "";
        int correct_idx = 1;
        
        if (hasData(data->sock, 1)) {
            std::string detail_resp = recvLine(data->sock);
            // Parse: ONE_QUESTION|id|content|opt1|opt2|opt3|opt4|correctIdx|difficulty|topic
            if (detail_resp.find("ONE_QUESTION|") == 0) {
                auto detail_parts = split(detail_resp.substr(13), '|'); // Skip "ONE_QUESTION|"
                if (detail_parts.size() >= 7) {
                    // detail_parts[0] = id, [1] = content, [2-5] = options, [6] = correctIdx
                    opt_a = detail_parts[2];
                    opt_b = detail_parts[3];
                    opt_c = detail_parts[4];
                    opt_d = detail_parts[5];
                    correct_idx = std::stoi(detail_parts[6]);
                }
            }
        }
        
        GtkTreeIter iter;
        gtk_list_store_append(data->list_store, &iter);
        gtk_list_store_set(data->list_store, &iter,
            QSTN_COL_ID, qid,
            QSTN_COL_CONTENT, content.c_str(),
            QSTN_COL_OPT_A, opt_a.c_str(),
            QSTN_COL_OPT_B, opt_b.c_str(),
            QSTN_COL_OPT_C, opt_c.c_str(),
            QSTN_COL_OPT_D, opt_d.c_str(),
            QSTN_COL_CORRECT, correct_idx,
            -1);
    }
}

// Callback: Quiz combo changed
static void on_quiz_combo_changed(GtkComboBox *combo, gpointer user_data) {
    QuestionTabData *data = (QuestionTabData*)user_data;
    
    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(combo, &iter)) {
        GtkTreeModel *model = gtk_combo_box_get_model(combo);
        int quiz_id;
        gtk_tree_model_get(model, &iter, 0, &quiz_id, -1);
        
        std::cout << "[Questions] Selected quiz " << quiz_id << std::endl;
        load_questions_for_quiz(data, quiz_id);
    }
}

// Refresh quiz combo
static void refresh_quiz_combo(QuestionTabData *data) {
    gtk_list_store_clear(data->quiz_store);
    
    sendLine(data->sock, "LIST_QUIZZES");
    if (!hasData(data->sock, 1)) return;
    
    std::string resp = recvLine(data->sock);
    if (resp.find("QUIZZES|") != 0) return;
    
    std::string quizData = resp.substr(8);
    if (quizData.empty()) return;
    
    auto items = split(quizData, ';');
    int index = 0;
    for (const auto& item : items) {
        if (item.empty()) continue;
        size_t colonPos = item.find(':');
        if (colonPos == std::string::npos) continue;
        
        int quiz_id = std::stoi(item.substr(0, colonPos));
        std::string rest = item.substr(colonPos + 1);
        size_t openParen = rest.find('(');
        std::string title = rest.substr(0, openParen);
        
        GtkTreeIter iter;
        gtk_list_store_append(data->quiz_store, &iter);
        gtk_list_store_set(data->quiz_store, &iter, 0, quiz_id, 1, title.c_str(), -1);
        index++;
    }
    
    if (index > 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(data->quiz_combo), 0);
    }
}

// Callback: Refresh button
static void on_question_refresh_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    QuestionTabData *data = (QuestionTabData*)user_data;
    refresh_quiz_combo(data);
}

// Callback: Add question button
static void on_question_add_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    QuestionTabData *data = (QuestionTabData*)user_data;
    
    if (data->current_quiz_id <= 0) {
        GtkWidget *err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "Vui lòng chọn quiz trước!");
        gtk_dialog_run(GTK_DIALOG(err));
        gtk_widget_destroy(err);
        return;
    }
    
    // Create dialog
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Thêm câu hỏi mới",
        NULL,
        GTK_DIALOG_MODAL,
        "Hủy", GTK_RESPONSE_CANCEL,
        "Thêm", GTK_RESPONSE_OK,
        NULL
    );
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 500);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 15);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    
    int row = 0;
    
    // Question content
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Câu hỏi:"), 0, row, 1, 1);
    GtkWidget *content_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(content_entry), "VD: C++ là ngôn ngữ gì?");
    gtk_widget_set_hexpand(content_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), content_entry, 1, row++, 1, 1);
    
    // Option A
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Đáp án A:"), 0, row, 1, 1);
    GtkWidget *opt_a = gtk_entry_new();
    gtk_widget_set_hexpand(opt_a, TRUE);
    gtk_grid_attach(GTK_GRID(grid), opt_a, 1, row++, 1, 1);
    
    // Option B
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Đáp án B:"), 0, row, 1, 1);
    GtkWidget *opt_b = gtk_entry_new();
    gtk_widget_set_hexpand(opt_b, TRUE);
    gtk_grid_attach(GTK_GRID(grid), opt_b, 1, row++, 1, 1);
    
    // Option C
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Đáp án C:"), 0, row, 1, 1);
    GtkWidget *opt_c = gtk_entry_new();
    gtk_widget_set_hexpand(opt_c, TRUE);
    gtk_grid_attach(GTK_GRID(grid), opt_c, 1, row++, 1, 1);
    
    // Option D
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Đáp án D:"), 0, row, 1, 1);
    GtkWidget *opt_d = gtk_entry_new();
    gtk_widget_set_hexpand(opt_d, TRUE);
    gtk_grid_attach(GTK_GRID(grid), opt_d, 1, row++, 1, 1);
    
    // Correct answer
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Đáp án đúng:"), 0, row, 1, 1);
    GtkWidget *correct_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(correct_combo), "A");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(correct_combo), "B");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(correct_combo), "C");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(correct_combo), "D");
    gtk_combo_box_set_active(GTK_COMBO_BOX(correct_combo), 0);
    gtk_grid_attach(GTK_GRID(grid), correct_combo, 1, row++, 1, 1);
    
    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_show_all(dialog);
    
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (response == GTK_RESPONSE_OK) {
        const char *question = gtk_entry_get_text(GTK_ENTRY(content_entry));
        const char *a = gtk_entry_get_text(GTK_ENTRY(opt_a));
        const char *b = gtk_entry_get_text(GTK_ENTRY(opt_b));
        const char *c = gtk_entry_get_text(GTK_ENTRY(opt_c));
        const char *d = gtk_entry_get_text(GTK_ENTRY(opt_d));
        int correct_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(correct_combo)) + 1; // 1-4
        
        if (strlen(question) == 0 || strlen(a) == 0 || strlen(b) == 0 || 
            strlen(c) == 0 || strlen(d) == 0) {
            gtk_widget_destroy(dialog);
            
            GtkWidget *err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Vui lòng điền đầy đủ thông tin!");
            gtk_dialog_run(GTK_DIALOG(err));
            gtk_widget_destroy(err);
            return;
        }
        
        // Send: ADD_QUESTION|quizId|content|opt1|opt2|opt3|opt4|correctIndex
        std::stringstream ss;
        ss << "ADD_QUESTION|" << data->current_quiz_id << "|" 
           << question << "|" << a << "|" << b << "|" << c << "|" << d << "|" << correct_idx;
        sendLine(data->sock, ss.str());
        
        if (hasData(data->sock, 2)) {
            std::string resp = recvLine(data->sock);
            std::cout << "[Question] Add response: " << resp << std::endl;
            
            if (resp.find("ADD_QUESTION_OK") == 0) {
                load_questions_for_quiz(data, data->current_quiz_id);
            } else {
                GtkWidget *err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Thêm câu hỏi thất bại:\n%s", resp.c_str());
                gtk_dialog_run(GTK_DIALOG(err));
                gtk_widget_destroy(err);
            }
        }
    }
    
    gtk_widget_destroy(dialog);
}

// Callback: Edit question button
static void on_question_edit_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    QuestionTabData *data = (QuestionTabData*)user_data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeIter iter;
    GtkTreeModel *model;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }
    
    int question_id, correct_idx;
    gchar *content, *opt_a, *opt_b, *opt_c, *opt_d;
    
    gtk_tree_model_get(model, &iter,
        QSTN_COL_ID, &question_id,
        QSTN_COL_CONTENT, &content,
        QSTN_COL_OPT_A, &opt_a,
        QSTN_COL_OPT_B, &opt_b,
        QSTN_COL_OPT_C, &opt_c,
        QSTN_COL_OPT_D, &opt_d,
        QSTN_COL_CORRECT, &correct_idx,
        -1);
    
    // Ensure no NULL pointers (GTK requires valid strings)
    if (!content) content = g_strdup("");
    if (!opt_a) opt_a = g_strdup("");
    if (!opt_b) opt_b = g_strdup("");
    if (!opt_c) opt_c = g_strdup("");
    if (!opt_d) opt_d = g_strdup("");
    
    // Create dialog
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Sửa câu hỏi",
        NULL,
        GTK_DIALOG_MODAL,
        "Hủy", GTK_RESPONSE_CANCEL,
        "Lưu", GTK_RESPONSE_OK,
        NULL
    );
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 500);
    
    GtkWidget *dlg_content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(dlg_content), 15);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    
    int row = 0;
    
    // Question content
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Câu hỏi:"), 0, row, 1, 1);
    GtkWidget *content_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(content_entry), content);
    gtk_widget_set_hexpand(content_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), content_entry, 1, row++, 1, 1);
    
    // Option A
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Đáp án A:"), 0, row, 1, 1);
    GtkWidget *opt_a_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(opt_a_entry), opt_a);
    gtk_widget_set_hexpand(opt_a_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), opt_a_entry, 1, row++, 1, 1);
    
    // Option B
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Đáp án B:"), 0, row, 1, 1);
    GtkWidget *opt_b_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(opt_b_entry), opt_b);
    gtk_widget_set_hexpand(opt_b_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), opt_b_entry, 1, row++, 1, 1);
    
    // Option C
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Đáp án C:"), 0, row, 1, 1);
    GtkWidget *opt_c_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(opt_c_entry), opt_c);
    gtk_widget_set_hexpand(opt_c_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), opt_c_entry, 1, row++, 1, 1);
    
    // Option D
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Đáp án D:"), 0, row, 1, 1);
    GtkWidget *opt_d_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(opt_d_entry), opt_d);
    gtk_widget_set_hexpand(opt_d_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), opt_d_entry, 1, row++, 1, 1);
    
    // Correct answer
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Đáp án đúng:"), 0, row, 1, 1);
    GtkWidget *correct_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(correct_combo), "A");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(correct_combo), "B");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(correct_combo), "C");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(correct_combo), "D");
    gtk_combo_box_set_active(GTK_COMBO_BOX(correct_combo), correct_idx - 1);
    gtk_grid_attach(GTK_GRID(grid), correct_combo, 1, row++, 1, 1);
    
    gtk_container_add(GTK_CONTAINER(dlg_content), grid);
    gtk_widget_show_all(dialog);
    
    int dlg_response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (dlg_response == GTK_RESPONSE_OK) {
        const char *new_content = gtk_entry_get_text(GTK_ENTRY(content_entry));
        const char *new_a = gtk_entry_get_text(GTK_ENTRY(opt_a_entry));
        const char *new_b = gtk_entry_get_text(GTK_ENTRY(opt_b_entry));
        const char *new_c = gtk_entry_get_text(GTK_ENTRY(opt_c_entry));
        const char *new_d = gtk_entry_get_text(GTK_ENTRY(opt_d_entry));
        int new_correct = gtk_combo_box_get_active(GTK_COMBO_BOX(correct_combo)) + 1;
        
        // Send: EDIT_QUESTION|qId|content|opt1|opt2|opt3|opt4|correctIndex
        std::stringstream edit_ss;
        edit_ss << "EDIT_QUESTION|" << question_id << "|" 
                << new_content << "|" << new_a << "|" << new_b << "|" 
                << new_c << "|" << new_d << "|" << new_correct;
        sendLine(data->sock, edit_ss.str());
        
        if (hasData(data->sock, 2)) {
            std::string edit_resp = recvLine(data->sock);
            std::cout << "[Question] Edit response: " << edit_resp << std::endl;
            
            if (edit_resp.find("EDIT_QUESTION_OK") == 0) {
                load_questions_for_quiz(data, data->current_quiz_id);
            } else {
                GtkWidget *err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Sửa câu hỏi thất bại!");
                gtk_dialog_run(GTK_DIALOG(err));
                gtk_widget_destroy(err);
            }
        }
    }
    
    gtk_widget_destroy(dialog);
    g_free(content);
    g_free(opt_a);
    g_free(opt_b);
    g_free(opt_c);
    g_free(opt_d);
}

// Callback: Delete question button
static void on_question_delete_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    QuestionTabData *data = (QuestionTabData*)user_data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeIter iter;
    GtkTreeModel *model;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        return;
    }
    
    int question_id;
    gchar *content;
    gtk_tree_model_get(model, &iter, 
        QSTN_COL_ID, &question_id,
        QSTN_COL_CONTENT, &content,
        -1);
    
    // Confirmation
    GtkWidget *dialog = gtk_message_dialog_new(
        NULL, GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
        "Xác nhận xóa câu hỏi?\n\n%s", content
    );
    
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    
    if (response == GTK_RESPONSE_YES) {
        std::string msg = "DELETE_QUESTION|" + std::to_string(question_id);
        sendLine(data->sock, msg);
        
        if (hasData(data->sock, 1)) {
            std::string resp = recvLine(data->sock);
            std::cout << "[Question] Delete response: " << resp << std::endl;
            
            if (resp.find("DELETE_QUESTION_OK") == 0) {
                load_questions_for_quiz(data, data->current_quiz_id);
            }
        }
    }
    
    g_free(content);
}

GtkWidget* createQuestionTab(int sock) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
    
    QuestionTabData *tabData = new QuestionTabData();
    tabData->sock = sock;
    tabData->current_quiz_id = 0;
    
    // Header
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    GtkWidget *label = gtk_label_new("Chọn quiz:");
    gtk_box_pack_start(GTK_BOX(header), label, FALSE, FALSE, 0);
    
    // Quiz combo box
    tabData->quiz_store = gtk_list_store_new(2, G_TYPE_INT, G_TYPE_STRING);
    tabData->quiz_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(tabData->quiz_store));
    
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(tabData->quiz_combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(tabData->quiz_combo), renderer, "text", 1, NULL);
    
    gtk_widget_set_size_request(tabData->quiz_combo, 300, -1);
    gtk_box_pack_start(GTK_BOX(header), tabData->quiz_combo, FALSE, FALSE, 0);
    
    // Refresh button
    GtkWidget *refresh_btn = gtk_button_new_with_label("🔄 Refresh");
    gtk_widget_set_size_request(refresh_btn, 100, 35);
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_question_refresh_clicked), tabData);
    gtk_box_pack_start(GTK_BOX(header), refresh_btn, FALSE, FALSE, 0);
    
    // Load quizzes into combo
    refresh_quiz_combo(tabData);
    
    // Connect combo changed signal
    g_signal_connect(tabData->quiz_combo, "changed", G_CALLBACK(on_quiz_combo_changed), tabData);
    
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);
    
    // TreeView for questions
    tabData->list_store = gtk_list_store_new(QSTN_NUM_COLS, 
        G_TYPE_INT,    // ID
        G_TYPE_STRING, // Content
        G_TYPE_STRING, // Opt A
        G_TYPE_STRING, // Opt B
        G_TYPE_STRING, // Opt C
        G_TYPE_STRING, // Opt D
        G_TYPE_INT);   // Correct index
    tabData->tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tabData->list_store));
    
    GtkTreeViewColumn *column;
    
    // ID
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("ID", renderer, "text", QSTN_COL_ID, NULL);
    gtk_tree_view_column_set_min_width(column, 50);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), column);
    
    // Content
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Nội dung câu hỏi", renderer, "text", QSTN_COL_CONTENT, NULL);
    gtk_tree_view_column_set_min_width(column, 500);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), column);
    
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), tabData->tree_view);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    
    // Buttons
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    GtkWidget *add_btn = gtk_button_new_with_label("➕ Thêm câu hỏi");
    gtk_widget_set_size_request(add_btn, 120, 35);
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_question_add_clicked), tabData);
    gtk_box_pack_start(GTK_BOX(btn_box), add_btn, FALSE, FALSE, 0);
    
    GtkWidget *edit_btn = gtk_button_new_with_label("✏️ Sửa");
    gtk_widget_set_size_request(edit_btn, 100, 35);
    g_signal_connect(edit_btn, "clicked", G_CALLBACK(on_question_edit_clicked), tabData);
    gtk_box_pack_start(GTK_BOX(btn_box), edit_btn, FALSE, FALSE, 0);
    
    GtkWidget *delete_btn = gtk_button_new_with_label("🗑️ Xóa");
    gtk_widget_set_size_request(delete_btn, 100, 35);
    g_signal_connect(delete_btn, "clicked", G_CALLBACK(on_question_delete_clicked), tabData);
    gtk_box_pack_start(GTK_BOX(btn_box), delete_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 0);
    
    return vbox;
}
