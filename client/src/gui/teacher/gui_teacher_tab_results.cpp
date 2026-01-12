#include "../../../include/gui/teacher/gui_teacher_tab_results.h"
#include "../../../include/client.h"
#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Chart data structure
struct ChartData {
    std::map<std::string, int> distribution;  // "0-2" -> count
    double avgScore;
    double maxScore;
    int totalStudents;
    std::string quizTitle;
    bool hasData;
    
    ChartData() : avgScore(0), maxScore(0), totalStudents(0), hasData(false) {}
};

// Exam Results tab data
struct ResultsTabData {
    GtkWidget *quiz_filter_combo;
    GtkWidget *chart_area;
    GtkWidget *placeholder_label;
    GtkWidget *stats_label;
    GtkListStore *quiz_store;
    ChartData *chartData;
    int sock;
};

// Draw chart callback
static gboolean on_chart_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    (void)widget;
    ResultsTabData *data = (ResultsTabData*)user_data;
    ChartData *chart = data->chartData;
    
    if (!chart || !chart->hasData) {
        return FALSE;
    }
    
    // Chart dimensions
    int width = gtk_widget_get_allocated_width(widget);
    int height = gtk_widget_get_allocated_height(widget);
    int margin_left = 60;
    int margin_right = 40;
    int margin_top = 50;
    int margin_bottom = 60;
    
    int chart_width = width - margin_left - margin_right;
    int chart_height = height - margin_top - margin_bottom;
    
    // Background
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);
    
    // Title
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 16);
    cairo_move_to(cr, margin_left, 30);
    cairo_show_text(cr, "Thống kê");
    
    // Find max count for scaling
    int maxCount = 0;
    std::vector<std::string> ranges = {"0-2", "2-4", "4-6", "6-8", "8-10"};
    for (const auto& range : ranges) {
        if (chart->distribution[range] > maxCount) {
            maxCount = chart->distribution[range];
        }
    }
    
    if (maxCount == 0) maxCount = 1;  // Avoid division by zero
    
    // Draw axes
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_line_width(cr, 2);
    cairo_move_to(cr, margin_left, margin_top);
    cairo_line_to(cr, margin_left, margin_top + chart_height);
    cairo_line_to(cr, margin_left + chart_width, margin_top + chart_height);
    cairo_stroke(cr);
    
    // Draw Y-axis labels and grid lines
    cairo_set_font_size(cr, 10);
    cairo_set_line_width(cr, 0.5);
    for (int i = 0; i <= 5; i++) {
        int value = (maxCount * i) / 5;
        int y = margin_top + chart_height - (chart_height * i) / 5;
        
        // Grid line
        cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
        cairo_move_to(cr, margin_left, y);
        cairo_line_to(cr, margin_left + chart_width, y);
        cairo_stroke(cr);
        
        // Label
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        char label[16];
        snprintf(label, sizeof(label), "%d", value);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, label, &extents);
        cairo_move_to(cr, margin_left - extents.width - 10, y + 4);
        cairo_show_text(cr, label);
    }
    
    // Draw bars
    int barCount = 5;
    int barWidth = (chart_width - 40) / barCount;
    int barSpacing = 10;
    
    for (int i = 0; i < barCount; i++) {
        std::string range = ranges[i];
        int count = chart->distribution[range];
        
        int x = margin_left + 20 + i * barWidth;
        int barHeight = (count * chart_height) / maxCount;
        int y = margin_top + chart_height - barHeight;
        
        // Draw bar
        cairo_set_source_rgb(cr, 0.29, 0.56, 0.89);  // Blue color
        cairo_rectangle(cr, x, y, barWidth - barSpacing, barHeight);
        cairo_fill(cr);
        
        // Draw border
        cairo_set_source_rgb(cr, 0.2, 0.4, 0.7);
        cairo_set_line_width(cr, 1);
        cairo_rectangle(cr, x, y, barWidth - barSpacing, barHeight);
        cairo_stroke(cr);
        
        // Draw count on top of bar
        if (count > 0) {
            cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
            cairo_set_font_size(cr, 12);
            char countStr[16];
            snprintf(countStr, sizeof(countStr), "%d", count);
            cairo_text_extents_t extents;
            cairo_text_extents(cr, countStr, &extents);
            cairo_move_to(cr, x + (barWidth - barSpacing - extents.width) / 2, y - 5);
            cairo_show_text(cr, countStr);
        }
        
        // Draw X-axis label
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_set_font_size(cr, 10);
        cairo_text_extents_t extents;
        cairo_text_extents(cr, range.c_str(), &extents);
        cairo_move_to(cr, x + (barWidth - barSpacing - extents.width) / 2, 
                     margin_top + chart_height + 20);
        cairo_show_text(cr, range.c_str());
    }
    
    // X-axis title
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_set_font_size(cr, 11);
    const char* xLabel = "Khoảng điểm";
    cairo_text_extents_t extents;
    cairo_text_extents(cr, xLabel, &extents);
    cairo_move_to(cr, margin_left + (chart_width - extents.width) / 2, 
                 height - 15);
    cairo_show_text(cr, xLabel);
    
    // Y-axis title (rotated)
    cairo_save(cr);
    cairo_move_to(cr, 15, margin_top + chart_height / 2);
    cairo_rotate(cr, -M_PI / 2);
    const char* yLabel = "Số học sinh";
    cairo_text_extents(cr, yLabel, &extents);
    cairo_show_text(cr, yLabel);
    cairo_restore(cr);
    
    return TRUE;
}

// Load quiz stats
static void load_quiz_stats(ResultsTabData *data, int quiz_id) {
    if (quiz_id <= 0) {
        // Show placeholder
        gtk_widget_show(data->placeholder_label);
        gtk_widget_hide(data->chart_area);
        gtk_widget_hide(data->stats_label);
        data->chartData->hasData = false;
        return;
    }
    
    // Request stats from server
    sendLine(data->sock, "GET_QUIZ_STATS|" + std::to_string(quiz_id));
    if (!hasData(data->sock, 1)) return;
    
    std::string resp = recvLine(data->sock);
    std::cout << "[Results] Stats: " << resp << std::endl;
    
    // Parse: QUIZ_STATS|quiz_id|avg|max|total|0-2:count1,2-4:count2,...
    auto parts = split(resp, '|');
    if (parts.size() < 6 || parts[0] != "QUIZ_STATS") {
        std::cerr << "[Results] Invalid stats response\n";
        return;
    }
    
    try {
        data->chartData->avgScore = std::stod(parts[2]);
        data->chartData->maxScore = std::stod(parts[3]);
        data->chartData->totalStudents = std::stoi(parts[4]);
        
        // Parse distribution
        data->chartData->distribution.clear();
        auto distParts = split(parts[5], ',');
        for (const auto& item : distParts) {
            auto rangeParts = split(item, ':');
            if (rangeParts.size() == 2) {
                std::string range = rangeParts[0];
                int count = std::stoi(rangeParts[1]);
                data->chartData->distribution[range] = count;
            }
        }
        
        data->chartData->hasData = true;
        
        // Update stats label
        char statsText[256];
        snprintf(statsText, sizeof(statsText),
                 "<b>Điểm trung bình:</b> %.1f  |  <b>Điểm cao nhất:</b> %.1f  |  <b>Tổng học sinh:</b> %d",
                 data->chartData->avgScore, data->chartData->maxScore, data->chartData->totalStudents);
        gtk_label_set_markup(GTK_LABEL(data->stats_label), statsText);
        
        // Show chart
        gtk_widget_hide(data->placeholder_label);
        gtk_widget_show(data->chart_area);
        gtk_widget_show(data->stats_label);
        gtk_widget_queue_draw(data->chart_area);
        
    } catch (...) {
        std::cerr << "[Results] Failed to parse stats\n";
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
    
    load_quiz_stats(data, quiz_id);
}

// Callback: Refresh button - reload quiz list and results
static void on_results_refresh_clicked(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    ResultsTabData *data = (ResultsTabData*)user_data;
    
    // Block the combo box signal temporarily to avoid recursive calls
    g_signal_handlers_block_by_func(data->quiz_filter_combo, (void*)on_results_filter_changed, data);
    
    // Reload quiz list
    gtk_list_store_clear(data->quiz_store);
    
    GtkTreeIter iter;
    gtk_list_store_append(data->quiz_store, &iter);
    gtk_list_store_set(data->quiz_store, &iter, 0, 0, 1, "Tất cả", -1);
    
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
                
                gtk_list_store_append(data->quiz_store, &iter);
                gtk_list_store_set(data->quiz_store, &iter, 0, quiz_id, 1, title.c_str(), -1);
            }
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(data->quiz_filter_combo), 0);
    
    // Unblock the signal
    g_signal_handlers_unblock_by_func(data->quiz_filter_combo, (void*)on_results_filter_changed, data);
    
    // Reload stats - always reset to "Chọn quiz..."
    load_quiz_stats(data, 0);
}

GtkWidget* createResultsTab(int sock) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 15);
    
    ResultsTabData *tabData = new ResultsTabData();
    tabData->sock = sock;
    tabData->chartData = new ChartData();
    
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
    
    // Load quizzes - always start with "Chọn quiz..."
    GtkTreeIter iter;
    gtk_list_store_append(tabData->quiz_store, &iter);
    gtk_list_store_set(tabData->quiz_store, &iter, 0, 0, 1, "--- Chọn quiz ---", -1);
    
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
    
    // Add refresh button
    GtkWidget *refresh_btn = gtk_button_new_with_label("🔄 Làm mới");
    gtk_widget_set_size_request(refresh_btn, 120, 35);
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_results_refresh_clicked), tabData);
    gtk_box_pack_start(GTK_BOX(header), refresh_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 5);
    
    // Placeholder label (shown when no quiz selected)
    tabData->placeholder_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(tabData->placeholder_label),
                        "<span size='x-large'>📊 Vui lòng chọn quiz để xem thống kê</span>");
    gtk_widget_set_valign(tabData->placeholder_label, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(tabData->placeholder_label, TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), tabData->placeholder_label, TRUE, TRUE, 0);
    
    // Chart area
    tabData->chart_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(tabData->chart_area, 700, 400);
    g_signal_connect(tabData->chart_area, "draw", G_CALLBACK(on_chart_draw), tabData);
    gtk_box_pack_start(GTK_BOX(vbox), tabData->chart_area, TRUE, TRUE, 0);
    gtk_widget_set_no_show_all(tabData->chart_area, TRUE);  // Don't show by default
    
    // Stats label (shown below chart)
    tabData->stats_label = gtk_label_new(NULL);
    gtk_widget_set_halign(tabData->stats_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(tabData->stats_label, 10);
    gtk_box_pack_start(GTK_BOX(vbox), tabData->stats_label, FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(tabData->stats_label, TRUE);  // Don't show by default
    
    return vbox;
}
