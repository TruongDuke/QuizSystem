#include "../../include/gui/gui_room_list.h"
#include "../../include/gui/gui_history.h"
#include "../../include/client.h"
#include <iostream>
#include <sstream>

// Cấu trúc dữ liệu cho window
struct RoomListData {
    GtkWidget *window;
    GtkWidget *tree_view;
    GtkListStore *list_store;
    RoomListResult *result;
    int sock;
};

// Enum cho columns
enum {
    COL_EXAM_ID = 0,
    COL_TITLE,
    COL_DESCRIPTION,
    COL_QUESTIONS,
    COL_TIME_LIMIT,
    COL_STATUS,
    NUM_COLS
};

// Load danh sách phòng từ server
static void load_exam_rooms(RoomListData *data) {
    // Gửi request LIST_QUIZZES
    sendLine(data->sock, "LIST_QUIZZES");
    
    std::string response = recvLine(data->sock);
    
    if (response.empty()) {
        std::cerr << "No response from server\n";
        return;
    }
    
    std::cout << "Server response: " << response << std::endl;
    
    // Clear list store
    gtk_list_store_clear(data->list_store);
    
    // Parse format: QUIZZES|1:Title(10Q,600s,status);2:Title2(5Q,300s,status);...
    auto parts = split(response, '|');
    if (parts.size() < 2 || parts[0] != "QUIZZES") {
        std::cerr << "Failed to get quiz list\n";
        return;
    }
    
    // Split by semicolon to get each quiz
    std::string quizzesData = parts[1];
    size_t pos = 0;
    int count = 0;
    
    while ((pos = quizzesData.find(';')) != std::string::npos) {
        std::string quizToken = quizzesData.substr(0, pos);
        
        // Parse: 1:Câu hỏi về C++(10Q,600s,not_started)
        size_t colonPos = quizToken.find(':');
        if (colonPos != std::string::npos) {
            std::string quizId = quizToken.substr(0, colonPos);
            std::string rest = quizToken.substr(colonPos + 1);
            
            // Find opening parenthesis
            size_t openParen = rest.find('(');
            if (openParen != std::string::npos) {
                std::string title = rest.substr(0, openParen);
                std::string info = rest.substr(openParen + 1, rest.length() - openParen - 2);
                
                // Parse info: 10Q,600s,not_started
                auto infoParts = split(info, ',');
                if (infoParts.size() >= 3) {
                    int questionCount = std::stoi(infoParts[0].substr(0, infoParts[0].length() - 1));
                    int timeLimit = std::stoi(infoParts[1].substr(0, infoParts[1].length() - 1));
                    std::string status = infoParts[2];
                    
                    GtkTreeIter iter;
                    gtk_list_store_append(data->list_store, &iter);
                    
                    gtk_list_store_set(data->list_store, &iter,
                        COL_EXAM_ID, std::stoi(quizId),
                        COL_TITLE, title.c_str(),
                        COL_DESCRIPTION, "",
                        COL_QUESTIONS, questionCount,
                        COL_TIME_LIMIT, timeLimit,
                        COL_STATUS, status.c_str(),
                        -1);
                    count++;
                }
            }
        }
        
        quizzesData.erase(0, pos + 1);
    }
    
    // Process last quiz
    if (!quizzesData.empty()) {
        size_t colonPos = quizzesData.find(':');
        if (colonPos != std::string::npos) {
            std::string quizId = quizzesData.substr(0, colonPos);
            std::string rest = quizzesData.substr(colonPos + 1);
            
            size_t openParen = rest.find('(');
            if (openParen != std::string::npos) {
                std::string title = rest.substr(0, openParen);
                std::string info = rest.substr(openParen + 1, rest.length() - openParen - 2);
                
                auto infoParts = split(info, ',');
                if (infoParts.size() >= 3) {
                    int questionCount = std::stoi(infoParts[0].substr(0, infoParts[0].length() - 1));
                    int timeLimit = std::stoi(infoParts[1].substr(0, infoParts[1].length() - 1));
                    std::string status = infoParts[2];
                    
                    GtkTreeIter iter;
                    gtk_list_store_append(data->list_store, &iter);
                    
                    gtk_list_store_set(data->list_store, &iter,
                        COL_EXAM_ID, std::stoi(quizId),
                        COL_TITLE, title.c_str(),
                        COL_DESCRIPTION, "",
                        COL_QUESTIONS, questionCount,
                        COL_TIME_LIMIT, timeLimit,
                        COL_STATUS, status.c_str(),
                        -1);
                    count++;
                }
            }
        }
    }
    
    std::cout << "Loaded " << count << " quizzes successfully\n";
}

// Callback khi nhấn nút Enter Room
static void on_enter_room_clicked(GtkWidget *widget, gpointer data) {
    RoomListData *room_data = (RoomListData*)data;
    
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(room_data->tree_view));
    GtkTreeModel *model;
    GtkTreeIter iter;
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gint examId;
        gchar *title;
        gint questionCount;
        
        gtk_tree_model_get(model, &iter,
            COL_EXAM_ID, &examId,
            COL_TITLE, &title,
            COL_QUESTIONS, &questionCount,
            -1);
        
        room_data->result->selectedRoom = true;
        room_data->result->examId = examId;
        room_data->result->title = title;
        room_data->result->questionCount = questionCount;
        
        g_free(title);
        gtk_main_quit();
    } else {
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(room_data->window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_OK,
            "Vui lòng chọn một phòng thi!"
        );
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
}

// Callback khi nhấn nút Refresh
static void on_refresh_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    RoomListData *room_data = (RoomListData*)data;
    load_exam_rooms(room_data);
}

// Callback khi nhấn nút History
static void on_history_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    RoomListData *room_data = (RoomListData*)data;
    showHistoryWindow(room_data->sock);
}

// Callback khi nhấn nút Logout
static void on_logout_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    RoomListData *room_data = (RoomListData*)data;
    room_data->result->selectedRoom = false;
    gtk_main_quit();
}

// Tạo TreeView
static GtkWidget* create_tree_view(RoomListData *data) {
    // Tạo list store
    data->list_store = gtk_list_store_new(NUM_COLS,
        G_TYPE_INT,      // exam_id
        G_TYPE_STRING,   // title
        G_TYPE_STRING,   // description
        G_TYPE_INT,      // question_count
        G_TYPE_INT,      // time_limit
        G_TYPE_STRING    // status
    );
    
    // Tạo tree view
    data->tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(data->list_store));
    
    // Thêm các columns
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    
    // Column: Exam ID (ẩn)
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("ID", renderer, "text", COL_EXAM_ID, NULL);
    gtk_tree_view_column_set_visible(column, FALSE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), column);
    
    // Column: Title
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Tên phòng thi", renderer, "text", COL_TITLE, NULL);
    gtk_tree_view_column_set_min_width(column, 200);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), column);
    
    // Column: Description
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Mô tả", renderer, "text", COL_DESCRIPTION, NULL);
    gtk_tree_view_column_set_min_width(column, 250);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), column);
    
    // Column: Questions
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Số câu", renderer, "text", COL_QUESTIONS, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), column);
    
    // Column: Time Limit
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Thời gian (s)", renderer, "text", COL_TIME_LIMIT, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), column);
    
    // Column: Status
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Trạng thái", renderer, "text", COL_STATUS, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(data->tree_view), column);
    
    return data->tree_view;
}

// Hàm chính hiển thị cửa sổ room list
RoomListResult showRoomListWindow(int sock) {
    RoomListResult result = {false, 0, "", 0};
    
    // Tạo cửa sổ
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Quiz System - Danh sách phòng thi");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 500);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    
    // Tạo RoomListData
    RoomListData data;
    data.window = window;
    data.result = &result;
    data.sock = sock;
    
    // Main container
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(vbox, 10);
    gtk_widget_set_margin_end(vbox, 10);
    gtk_widget_set_margin_top(vbox, 10);
    gtk_widget_set_margin_bottom(vbox, 10);
    
    // Tiêu đề
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), 
        "<span font='18' weight='bold'>DANH SÁCH PHÒNG THI</span>");
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 5);
    
    // Scrolled window cho tree view
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    
    // Tạo tree view
    GtkWidget *tree_view = create_tree_view(&data);
    gtk_container_add(GTK_CONTAINER(scrolled), tree_view);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled, TRUE, TRUE, 0);
    
    // Button box
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
    
    // Nút Refresh
    GtkWidget *refresh_btn = gtk_button_new_with_label("🔄 Làm mới");
    gtk_widget_set_size_request(refresh_btn, 120, 40);
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_refresh_clicked), &data);
    gtk_box_pack_start(GTK_BOX(button_box), refresh_btn, FALSE, FALSE, 5);
    
    // Nút Enter Room
    GtkWidget *enter_btn = gtk_button_new_with_label("✓ Vào phòng thi");
    gtk_widget_set_size_request(enter_btn, 150, 40);
    g_signal_connect(enter_btn, "clicked", G_CALLBACK(on_enter_room_clicked), &data);
    gtk_box_pack_start(GTK_BOX(button_box), enter_btn, FALSE, FALSE, 5);
    
    // Nút History
    GtkWidget *history_btn = gtk_button_new_with_label("📋 Lịch sử");
    gtk_widget_set_size_request(history_btn, 120, 40);
    g_signal_connect(history_btn, "clicked", G_CALLBACK(on_history_clicked), &data);
    gtk_box_pack_start(GTK_BOX(button_box), history_btn, FALSE, FALSE, 5);
    
    // Nút Logout
    GtkWidget *logout_btn = gtk_button_new_with_label("⊗ Đăng xuất");
    gtk_widget_set_size_request(logout_btn, 120, 40);
    g_signal_connect(logout_btn, "clicked", G_CALLBACK(on_logout_clicked), &data);
    gtk_box_pack_start(GTK_BOX(button_box), logout_btn, FALSE, FALSE, 5);
    
    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 10);
    
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    // Signal khi đóng cửa sổ
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Load dữ liệu
    load_exam_rooms(&data);
    
    // Hiển thị
    gtk_widget_show_all(window);
    gtk_main();
    
    // Destroy window trước khi return
    gtk_widget_destroy(window);
    
    // Process pending events
    while (gtk_events_pending())
        gtk_main_iteration();
    
    return result;
}
