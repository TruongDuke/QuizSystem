#include "../../../include/gui/teacher/gui_teacher_tab_bank.h"
#include "../../../include/client.h"
#include <iostream>
#include <sstream>

// Question Bank tab data
struct BankTabData {
    GtkWidget *difficulty_combo;
    GtkWidget *topic_combo;
    GtkWidget *quiz_combo;
    GtkWidget *tree_view;
    GtkListStore *list_store;
    GtkListStore *quiz_store;
    int sock;
    int selected_bank_id;
};

// Question Bank columns
enum {
    BANK_COL_ID,
    BANK_COL_CONTENT,
    BANK_COL_DIFFICULTY,
    BANK_COL_TOPIC,
    BANK_NUM_COLS
};

// Load questions from bank
static void load_question_bank(BankTabData *data) {
    gtk_list_store_clear(data->list_store);
    
    // Get filters
    gchar *difficulty = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(data->difficulty_combo));
    gchar *topic = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(data->topic_combo));
    
    std::string diff_filter = difficulty ? std::string(difficulty) : "all";
    std::string topic_filter = topic ? std::string(topic) : "all";
    
    g_free(difficulty);
    g_free(topic);
    
    // LIST_QUESTION_BANK|topic|difficulty
    std::string msg = "LIST_QUESTION_BANK|" + topic_filter + "|" + diff_filter;
    sendLine(data->sock, msg);
    
    if (!hasData(data->sock, 1)) return;
    
    std::string resp = recvLine(data->sock);
    std::cout << "[Bank] Server: " << resp << std::endl;
    
    // Parse: QUESTION_BANK|id:text[diff,topic];id2:text2[diff2,topic2];...
    if (resp.find("QUESTION_BANK|") != 0) return;
    
    std::string items_str = resp.substr(14); // Skip "QUESTION_BANK|"
    if (items_str.empty() || items_str == "No questions found") return;
    
    auto items = split(items_str, ';');
    for (const auto& item : items) {
        if (item.empty()) continue;
        
        // Parse: id:text[diff,topic]
        size_t colonPos = item.find(':');
        if (colonPos == std::string::npos) continue;
        
        int bid = std::stoi(item.substr(0, colonPos));
        std::string rest = item.substr(colonPos + 1);
        
        size_t bracketPos = rest.find('[');
        if (bracketPos == std::string::npos) continue;
        
        std::string text = rest.substr(0, bracketPos);
        std::string meta = rest.substr(bracketPos + 1);
        meta.pop_back(); // Remove ']'
        
        size_t commaPos = meta.find(',');
        std::string diff = meta.substr(0, commaPos);
        std::string topic = meta.substr(commaPos + 1);
        
        GtkTreeIter iter;
        gtk_list_store_append(data->list_store, &iter);
        gtk_list_store_set(data->list_store, &iter,
            BANK_COL_ID, bid,
            BANK_COL_CONTENT, text.c_str(),
            BANK_COL_DIFFICULTY, diff.c_str(),
            BANK_COL_TOPIC, topic.c_str(),
            -1);
    }
}

// Callback: Filter changed - reload both questions and quiz list
static void on_bank_filter_changed(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    BankTabData *data = (BankTabData*)user_data;
    
    // Reload questions
    load_question_bank(data);
    
    // Reload quiz list
    gtk_list_store_clear(data->quiz_store);
    sendLine(data->sock, "LIST_QUIZZES");
    if (hasData(data->sock, 1)) {
        std::string resp = recvLine(data->sock);
        if (resp.find("QUIZZES|") == 0) {
            auto items = split(resp.substr(8), ';');
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
            }
            gtk_combo_box_set_active(GTK_COMBO_BOX(data->quiz_combo), 0);
        }
    }
}

// Callback: Add to quiz button
static void on_add_to_quiz_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    BankTabData *data = (BankTabData*)user_data;
    
    // Get selected bank question
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->tree_view));
    GtkTreeIter iter;
    GtkTreeModel *model;
    
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        GtkWidget *err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "Vui lòng chọn câu hỏi!");
        gtk_dialog_run(GTK_DIALOG(err));
        gtk_widget_destroy(err);
        return;
    }
    
    int bank_id;
    gtk_tree_model_get(model, &iter, BANK_COL_ID, &bank_id, -1);
    
    // Get selected quiz
    GtkTreeIter quiz_iter;
    if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(data->quiz_combo), &quiz_iter)) {
        GtkWidget *err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "Vui lòng chọn quiz!");
        gtk_dialog_run(GTK_DIALOG(err));
        gtk_widget_destroy(err);
        return;
    }
    
    int quiz_id;
    gtk_tree_model_get(GTK_TREE_MODEL(data->quiz_store), &quiz_iter, 0, &quiz_id, -1);
    
    // ADD_TO_QUIZ_FROM_BANK|quizId|bankQuestionId
    std::string msg = "ADD_TO_QUIZ_FROM_BANK|" + std::to_string(quiz_id) + "|" + std::to_string(bank_id);
    sendLine(data->sock, msg);
    
    if (hasData(data->sock, 1)) {
        std::string resp = recvLine(data->sock);
        if (resp.find("ADD_TO_QUIZ_FROM_BANK_OK") == 0) {
            GtkWidget *info = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Đã thêm câu hỏi vào quiz!");
            gtk_dialog_run(GTK_DIALOG(info));
            gtk_widget_destroy(info);
        } else if (resp.find("quota_exceeded") != std::string::npos) {
            // Parse error message: ADD_TO_QUIZ_FROM_BANK_FAIL|reason=quota_exceeded|current=2|max=2
            size_t currentPos = resp.find("current=");
            size_t maxPos = resp.find("max=");
            std::string errorMsg = "Không thể thêm! Bài thi đã đủ số câu hỏi.";
            if (currentPos != std::string::npos && maxPos != std::string::npos) {
                std::string currentStr = resp.substr(currentPos + 8);
                std::string maxStr = resp.substr(maxPos + 4);
                size_t pipePos = currentStr.find('|');
                if (pipePos != std::string::npos) currentStr = currentStr.substr(0, pipePos);
                errorMsg = "Không thể thêm! Bài thi đã có " + currentStr + "/" + maxStr + " câu hỏi.";
            }
            GtkWidget *err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "%s", errorMsg.c_str());
            gtk_dialog_run(GTK_DIALOG(err));
            gtk_widget_destroy(err);
        } else {
            GtkWidget *err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Thêm thất bại: %s", resp.c_str());
            gtk_dialog_run(GTK_DIALOG(err));
            gtk_widget_destroy(err);
        }
    }
}

// Callback: Add new question to bank
static void on_add_to_bank_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    BankTabData *data = (BankTabData*)user_data;
    
    // Create dialog
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Thêm câu hỏi vào Bank",
        NULL,
        GTK_DIALOG_MODAL,
        "Hủy", GTK_RESPONSE_CANCEL,
        "Thêm", GTK_RESPONSE_OK,
        NULL
    );
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 550);
    
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 15);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    
    int row = 0;
    
    // Question content
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Câu hỏi:"), 0, row, 1, 1);
    GtkWidget *content_entry = gtk_entry_new();
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
    
    // Difficulty
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Độ khó:"), 0, row, 1, 1);
    GtkWidget *diff_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(diff_combo), "easy");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(diff_combo), "medium");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(diff_combo), "hard");
    gtk_combo_box_set_active(GTK_COMBO_BOX(diff_combo), 0);
    gtk_grid_attach(GTK_GRID(grid), diff_combo, 1, row++, 1, 1);
    
    // Topic
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Chủ đề:"), 0, row, 1, 1);
    GtkWidget *topic_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(topic_entry), "VD: Math, Programming, Database");
    gtk_widget_set_hexpand(topic_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), topic_entry, 1, row++, 1, 1);
    
    gtk_container_add(GTK_CONTAINER(content), grid);
    gtk_widget_show_all(dialog);
    
    int response = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (response == GTK_RESPONSE_OK) {
        const char *question = gtk_entry_get_text(GTK_ENTRY(content_entry));
        const char *a = gtk_entry_get_text(GTK_ENTRY(opt_a));
        const char *b = gtk_entry_get_text(GTK_ENTRY(opt_b));
        const char *c = gtk_entry_get_text(GTK_ENTRY(opt_c));
        const char *d = gtk_entry_get_text(GTK_ENTRY(opt_d));
        int correct_idx = gtk_combo_box_get_active(GTK_COMBO_BOX(correct_combo)) + 1;
        
        gchar *diff = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(diff_combo));
        const char *topic = gtk_entry_get_text(GTK_ENTRY(topic_entry));
        
        if (strlen(question) == 0 || strlen(a) == 0 || strlen(b) == 0 || 
            strlen(c) == 0 || strlen(d) == 0 || strlen(topic) == 0) {
            gtk_widget_destroy(dialog);
            
            GtkWidget *err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Vui lòng điền đầy đủ thông tin!");
            gtk_dialog_run(GTK_DIALOG(err));
            gtk_widget_destroy(err);
            g_free(diff);
            return;
        }
        
        // Send: ADD_TO_BANK|content|opt1|opt2|opt3|opt4|correctIndex|difficulty|topic
        std::stringstream ss;
        ss << "ADD_TO_BANK|" << question << "|" << a << "|" << b << "|" 
           << c << "|" << d << "|" << correct_idx << "|" << diff << "|" << topic;
        sendLine(data->sock, ss.str());
        
        g_free(diff);
        
        if (hasData(data->sock, 2)) {
            std::string resp = recvLine(data->sock);
            std::cout << "[Bank] Add response: " << resp << std::endl;
            
            if (resp.find("ADD_TO_BANK_OK") == 0) {
                load_question_bank(data);
                GtkWidget *info = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                    GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Đã thêm câu hỏi vào Bank!");
                gtk_dialog_run(GTK_DIALOG(info));
                gtk_widget_destroy(info);
            } else {
                GtkWidget *err = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL,
                    GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Thêm thất bại!");
                gtk_dialog_run(GTK_DIALOG(err));
                gtk_widget_destroy(err);
            }
        }
    }
    
    gtk_widget_destroy(dialog);
}

GtkWidget* createBankTab(int sock) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
    
    BankTabData *tabData = new BankTabData();
    tabData->sock = sock;
    tabData->selected_bank_id = 0;
    
    // Header with filters
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    // Difficulty filter
    gtk_box_pack_start(GTK_BOX(header), gtk_label_new("Độ khó:"), FALSE, FALSE, 0);
    tabData->difficulty_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tabData->difficulty_combo), "all");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tabData->difficulty_combo), "easy");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tabData->difficulty_combo), "medium");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tabData->difficulty_combo), "hard");
    gtk_combo_box_set_active(GTK_COMBO_BOX(tabData->difficulty_combo), 0);
    gtk_widget_set_size_request(tabData->difficulty_combo, 120, -1);
    g_signal_connect(tabData->difficulty_combo, "changed", G_CALLBACK(on_bank_filter_changed), tabData);
    gtk_box_pack_start(GTK_BOX(header), tabData->difficulty_combo, FALSE, FALSE, 0);
    
    // Topic filter
    gtk_box_pack_start(GTK_BOX(header), gtk_label_new("Chủ đề:"), FALSE, FALSE, 0);
    tabData->topic_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tabData->topic_combo), "all");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tabData->topic_combo), "Math");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tabData->topic_combo), "Programming");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(tabData->topic_combo), "Database");
    gtk_combo_box_set_active(GTK_COMBO_BOX(tabData->topic_combo), 0);
    gtk_widget_set_size_request(tabData->topic_combo, 150, -1);
    g_signal_connect(tabData->topic_combo, "changed", G_CALLBACK(on_bank_filter_changed), tabData);
    gtk_box_pack_start(GTK_BOX(header), tabData->topic_combo, FALSE, FALSE, 0);
    
    // Quiz selector
    gtk_box_pack_start(GTK_BOX(header), gtk_label_new("Thêm vào quiz:"), FALSE, FALSE, 20);
    tabData->quiz_store = gtk_list_store_new(2, G_TYPE_INT, G_TYPE_STRING);
    tabData->quiz_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(tabData->quiz_store));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(tabData->quiz_combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(tabData->quiz_combo), renderer, "text", 1, NULL);
    gtk_widget_set_size_request(tabData->quiz_combo, 200, -1);
    gtk_box_pack_start(GTK_BOX(header), tabData->quiz_combo, FALSE, FALSE, 0);
    
    // Load quizzes into combo
    sendLine(tabData->sock, "LIST_QUIZZES");
    if (hasData(tabData->sock, 1)) {
        std::string resp = recvLine(tabData->sock);
        if (resp.find("QUIZZES|") == 0) {
            auto items = split(resp.substr(8), ';');
            for (const auto& item : items) {
                if (item.empty()) continue;
                size_t colonPos = item.find(':');
                if (colonPos == std::string::npos) continue;
                int quiz_id = std::stoi(item.substr(0, colonPos));
                std::string rest = item.substr(colonPos + 1);
                size_t openParen = rest.find('(');
                std::string title = rest.substr(0, openParen);
                
                GtkTreeIter iter;
                gtk_list_store_append(tabData->quiz_store, &iter);
                gtk_list_store_set(tabData->quiz_store, &iter, 0, quiz_id, 1, title.c_str(), -1);
            }
            gtk_combo_box_set_active(GTK_COMBO_BOX(tabData->quiz_combo), 0);
        }
    }
    
    GtkWidget *add_btn = gtk_button_new_with_label("➕ Thêm vào quiz");
    gtk_widget_set_size_request(add_btn, 150, 35);
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_to_quiz_clicked), tabData);
    gtk_box_pack_start(GTK_BOX(header), add_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);
    
    // Buttons row
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(btn_box), 5);
    
    GtkWidget *refresh_btn = gtk_button_new_with_label("🔄 Làm mới");
    gtk_widget_set_size_request(refresh_btn, 120, 35);
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_bank_filter_changed), tabData);
    gtk_box_pack_start(GTK_BOX(btn_box), refresh_btn, FALSE, FALSE, 0);
    
    GtkWidget *add_bank_btn = gtk_button_new_with_label("➕ Thêm câu hỏi mới vào Bank");
    gtk_widget_set_size_request(add_bank_btn, 220, 35);
    g_signal_connect(add_bank_btn, "clicked", G_CALLBACK(on_add_to_bank_clicked), tabData);
    gtk_box_pack_start(GTK_BOX(btn_box), add_bank_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);
    
    // TreeView
    tabData->list_store = gtk_list_store_new(BANK_NUM_COLS, 
        G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    tabData->tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tabData->list_store));
    
    // Columns
    GtkCellRenderer *id_renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *id_col = gtk_tree_view_column_new_with_attributes(
        "ID", id_renderer, "text", BANK_COL_ID, NULL);
    gtk_tree_view_column_set_fixed_width(id_col, 50);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), id_col);
    
    GtkCellRenderer *content_renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *content_col = gtk_tree_view_column_new_with_attributes(
        "Câu hỏi", content_renderer, "text", BANK_COL_CONTENT, NULL);
    gtk_tree_view_column_set_expand(content_col, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), content_col);
    
    GtkCellRenderer *diff_renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *diff_col = gtk_tree_view_column_new_with_attributes(
        "Độ khó", diff_renderer, "text", BANK_COL_DIFFICULTY, NULL);
    gtk_tree_view_column_set_fixed_width(diff_col, 100);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), diff_col);
    
    GtkCellRenderer *topic_renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *topic_col = gtk_tree_view_column_new_with_attributes(
        "Chủ đề", topic_renderer, "text", BANK_COL_TOPIC, NULL);
    gtk_tree_view_column_set_fixed_width(topic_col, 150);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), topic_col);
    
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), tabData->tree_view);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    
    // Load initial data
    load_question_bank(tabData);
    
    return vbox;
}
