#include "../../../include/gui/student/gui_history.h"
#include "../../../include/client.h"
#include <iostream>
#include <vector>

// Exam history record
struct HistoryRecord {
    int examId;
    std::string quizTitle;
    int score;
    int correctAnswers;
    int totalQuestions;
    std::string date;
    std::string status;
};

// History window data
struct HistoryData {
    GtkWidget *window;
    GtkWidget *tree_view;
    GtkListStore *list_store;
    int sock;
};

// Columns
enum {
    COL_EXAM_ID,
    COL_QUIZ_TITLE,
    COL_SCORE,
    COL_STATUS,
    NUM_COLS
};

// Load history from server
static void load_history(HistoryData *data) {
    sendLine(data->sock, "LIST_MY_HISTORY");
    std::string response = recvLine(data->sock);
    
    std::cout << "[DEBUG] History response: " << response << std::endl;
    
    gtk_list_store_clear(data->list_store);
    
    // Format from server: HISTORY|title: scoređ;title2: score2đ;...
    // or: HISTORY|Chưa có lịch sử thi
    
    if (response.find("HISTORY|") != 0) {
        std::cerr << "Invalid history response\n";
        return;
    }
    
    std::string historyData = response.substr(8);
    if (historyData.empty() || historyData == "Chưa có lịch sử thi") {
        return; // No history
    }
    
    // Split by semicolon
    auto items = split(historyData, ';');
    int rowNum = 1;
    
    for (const auto& item : items) {
        if (item.empty()) continue;
        
        // Parse: "Title: scoređ"
        size_t colonPos = item.rfind(": "); // rfind để tìm dấu : cuối cùng
        if (colonPos != std::string::npos) {
            std::string title = item.substr(0, colonPos);
            std::string scoreStr = item.substr(colonPos + 2);
            
            // Remove "đ" suffix
            if (scoreStr.length() > 0 && (unsigned char)scoreStr.back() == 0xC4) {
                scoreStr.pop_back();
            }
            if (scoreStr.length() > 0 && (unsigned char)scoreStr.back() == 0x91) {
                scoreStr.pop_back();
            }
            
            // Simple: remove last char if not digit
            while (!scoreStr.empty() && !isdigit((unsigned char)scoreStr.back())) {
                scoreStr.pop_back();
            }
            
            int score = 0;
            try {
                if (!scoreStr.empty()) {
                    score = std::stoi(scoreStr);
                }
            } catch (...) {
                score = 0;
            }
            
            GtkTreeIter iter;
            gtk_list_store_append(data->list_store, &iter);
            gtk_list_store_set(data->list_store, &iter,
                COL_EXAM_ID, rowNum++,
                COL_QUIZ_TITLE, title.c_str(),
                COL_SCORE, score,
                COL_STATUS, "Đã nộp",
                -1);
        }
    }
}

// Callback: Close button
static void on_close_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    HistoryData *history = (HistoryData*)data;
    gtk_widget_destroy(history->window);
}

// Create tree view
static GtkWidget* create_tree_view(HistoryData *data) {
    // Create list store
    data->list_store = gtk_list_store_new(NUM_COLS,
        G_TYPE_INT,      // exam_id
        G_TYPE_STRING,   // quiz_title
        G_TYPE_INT,      // score

        G_TYPE_STRING    // status
    );
    
    // Create tree view
    data->tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(data->list_store));
    
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    
    // Column: Exam ID (hidden)
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("ID", renderer, "text", COL_EXAM_ID, NULL);
    gtk_tree_view_column_set_visible(column, FALSE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), column);
    
    // Column: Quiz Title
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Bài thi", renderer, "text", COL_QUIZ_TITLE, NULL);
    gtk_tree_view_column_set_min_width(column, 250);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), column);
    
    // Column: Score
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Điểm", renderer, "text", COL_SCORE, NULL);
    gtk_tree_view_column_set_min_width(column, 80);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), column);
    
    // Column: Status
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Trạng thái", renderer, "text", COL_STATUS, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), column);
    
    return data->tree_view;
}

// Main function
void showHistoryWindow(int sock) {
    // Create window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Lịch sử thi của bạn");
    gtk_window_set_default_size(GTK_WINDOW(window), 850, 500);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    
    HistoryData data;
    data.window = window;
    data.sock = sock;
    
    // Main container
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
    
    // Title
    GtkWidget *title_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title_label), 
        "<span size='x-large' weight='bold'>LỊCH SỬ THI</span>");
    gtk_box_pack_start(GTK_BOX(vbox), title_label, FALSE, FALSE, 10);
    
    // Separator
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);
    
    // Tree view in scrolled window
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    
    GtkWidget *tree = create_tree_view(&data);
    gtk_container_add(GTK_CONTAINER(scroll), tree);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    
    // Close button
    GtkWidget *close_btn = gtk_button_new_with_label("Đóng");
    gtk_widget_set_size_request(close_btn, 120, 40);
    gtk_widget_set_halign(close_btn, GTK_ALIGN_CENTER);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_close_clicked), &data);
    gtk_box_pack_start(GTK_BOX(vbox), close_btn, FALSE, FALSE, 10);
    
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    // Signals - chỉ quit main loop của history, không ảnh hưởng room_list
    g_signal_connect_swapped(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Load data
    load_history(&data);
    
    gtk_widget_show_all(window);
    gtk_main();
}
