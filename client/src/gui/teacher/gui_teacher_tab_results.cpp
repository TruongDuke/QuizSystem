#include "../../../include/gui/teacher/gui_teacher_tab_results.h"
#include "../../../include/client.h"
#include <iostream>
#include <sstream>

// Exam Results tab data
struct ResultsTabData {
    GtkWidget *quiz_filter_combo;
    GtkWidget *tree_view;
    GtkListStore *list_store;
    GtkListStore *quiz_store;
    int sock;
};

// Results columns
enum {
    RESULT_COL_EXAM_ID,
    RESULT_COL_STUDENT,
    RESULT_COL_QUIZ,
    RESULT_COL_SCORE,
    RESULT_COL_STATUS,
    RESULT_NUM_COLS
};

// Load exam results
static void load_exam_results(ResultsTabData *data, int quiz_filter) {
    gtk_list_store_clear(data->list_store);
    
    sendLine(data->sock, "LIST_EXAMS");
    if (!hasData(data->sock, 1)) return;
    
    std::string resp = recvLine(data->sock);
    std::cout << "[Results] Server: " << resp << std::endl;
    
    // Parse: EXAMS|eid(quiz=qid,user=uid,score=s,status=st);...
    if (resp.find("EXAMS|") != 0) return;
    
    std::string items_str = resp.substr(6);
    if (items_str.empty()) return;
    
    auto items = split(items_str, ';');
    for (const auto& item : items) {
        if (item.empty()) continue;
        
        // Parse: eid(quiz=qid,user=uid,score=s,status=st)
        size_t parenPos = item.find('(');
        if (parenPos == std::string::npos) continue;
        
        int exam_id = std::stoi(item.substr(0, parenPos));
        std::string meta = item.substr(parenPos + 1);
        meta.pop_back(); // Remove ')'
        
        // Parse meta: quiz=qid,user=uid,score=s,status=st
        int quiz_id = 0, user_id = 0;
        double score = 0.0;
        std::string status;
        
        auto meta_parts = split(meta, ',');
        for (const auto& part : meta_parts) {
            size_t eqPos = part.find('=');
            if (eqPos == std::string::npos) continue;
            std::string key = part.substr(0, eqPos);
            std::string val = part.substr(eqPos + 1);
            
            if (key == "quiz") quiz_id = std::stoi(val);
            else if (key == "user") user_id = std::stoi(val);
            else if (key == "score") score = std::stod(val);
            else if (key == "status") status = val;
        }
        
        // Filter by quiz if needed
        if (quiz_filter > 0 && quiz_id != quiz_filter) continue;
        
        // Get student name (simplified - just show user_id)
        std::string student_name = "User #" + std::to_string(user_id);
        
        // Get quiz title (simplified - just show quiz_id)
        std::string quiz_title = "Quiz #" + std::to_string(quiz_id);
        
        GtkTreeIter iter;
        gtk_list_store_append(data->list_store, &iter);
        gtk_list_store_set(data->list_store, &iter,
            RESULT_COL_EXAM_ID, exam_id,
            RESULT_COL_STUDENT, student_name.c_str(),
            RESULT_COL_QUIZ, quiz_title.c_str(),
            RESULT_COL_SCORE, score,
            RESULT_COL_STATUS, status.c_str(),
            -1);
    }
}

// Callback: Quiz filter changed
static void on_results_filter_changed(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    ResultsTabData *data = (ResultsTabData*)user_data;
    
    GtkTreeIter iter;
    int quiz_id = 0;
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(data->quiz_filter_combo), &iter)) {
        gtk_tree_model_get(GTK_TREE_MODEL(data->quiz_store), &iter, 0, &quiz_id, -1);
    }
    
    load_exam_results(data, quiz_id);
}

GtkWidget* createResultsTab(int sock) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
    
    ResultsTabData *tabData = new ResultsTabData();
    tabData->sock = sock;
    
    // Header with filter
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    gtk_box_pack_start(GTK_BOX(header), gtk_label_new("Lọc theo quiz:"), FALSE, FALSE, 0);
    
    // Quiz filter combo
    tabData->quiz_store = gtk_list_store_new(2, G_TYPE_INT, G_TYPE_STRING);
    tabData->quiz_filter_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(tabData->quiz_store));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(tabData->quiz_filter_combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(tabData->quiz_filter_combo), renderer, "text", 1, NULL);
    gtk_widget_set_size_request(tabData->quiz_filter_combo, 300, -1);
    g_signal_connect(tabData->quiz_filter_combo, "changed", G_CALLBACK(on_results_filter_changed), tabData);
    gtk_box_pack_start(GTK_BOX(header), tabData->quiz_filter_combo, FALSE, FALSE, 0);
    
    // Load quizzes
    GtkTreeIter iter;
    gtk_list_store_append(tabData->quiz_store, &iter);
    gtk_list_store_set(tabData->quiz_store, &iter, 0, 0, 1, "Tất cả", -1);
    
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
                
                gtk_list_store_append(tabData->quiz_store, &iter);
                gtk_list_store_set(tabData->quiz_store, &iter, 0, quiz_id, 1, title.c_str(), -1);
            }
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(tabData->quiz_filter_combo), 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);
    
    // TreeView
    tabData->list_store = gtk_list_store_new(RESULT_NUM_COLS,
        G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_STRING);
    tabData->tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(tabData->list_store));
    
    // Columns
    GtkCellRenderer *id_renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *id_col = gtk_tree_view_column_new_with_attributes(
        "Exam ID", id_renderer, "text", RESULT_COL_EXAM_ID, NULL);
    gtk_tree_view_column_set_fixed_width(id_col, 80);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), id_col);
    
    GtkCellRenderer *student_renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *student_col = gtk_tree_view_column_new_with_attributes(
        "Học sinh", student_renderer, "text", RESULT_COL_STUDENT, NULL);
    gtk_tree_view_column_set_expand(student_col, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), student_col);
    
    GtkCellRenderer *quiz_renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *quiz_col = gtk_tree_view_column_new_with_attributes(
        "Quiz", quiz_renderer, "text", RESULT_COL_QUIZ, NULL);
    gtk_tree_view_column_set_expand(quiz_col, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), quiz_col);
    
    GtkCellRenderer *score_renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *score_col = gtk_tree_view_column_new_with_attributes(
        "Điểm", score_renderer, "text", RESULT_COL_SCORE, NULL);
    gtk_tree_view_column_set_fixed_width(score_col, 80);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), score_col);
    
    GtkCellRenderer *status_renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *status_col = gtk_tree_view_column_new_with_attributes(
        "Trạng thái", status_renderer, "text", RESULT_COL_STATUS, NULL);
    gtk_tree_view_column_set_fixed_width(status_col, 120);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tabData->tree_view), status_col);
    
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), tabData->tree_view);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    
    // Load initial data
    load_exam_results(tabData, 0);
    
    return vbox;
}
