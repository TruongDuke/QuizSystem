#include "../../../include/gui/teacher/gui_teacher_main.h"
#include "../../../include/gui/teacher/gui_teacher_tab_quiz.h"
#include "../../../include/gui/teacher/gui_teacher_tab_question.h"
#include "../../../include/gui/teacher/gui_teacher_tab_bank.h"
#include "../../../include/gui/teacher/gui_teacher_tab_results.h"
#include "../../../include/client.h"
#include <iostream>

// Teacher dashboard data
struct TeacherData {
    GtkWidget *window;
    GtkWidget *notebook;
    int sock;
    bool shouldLogout;
};

// Callback: Logout button
static void on_logout_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    TeacherData *teacher = (TeacherData*)data;
    teacher->shouldLogout = true;
    gtk_widget_destroy(teacher->window);
}

// Main function: Show teacher dashboard
void showTeacherDashboard(int sock) {
    std::cout << "[Teacher Dashboard] Starting...\n";
    
    // Create window
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Teacher Dashboard - Quiz System");
    gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    
    TeacherData data;
    data.window = window;
    data.sock = sock;
    data.shouldLogout = false;
    
    // Main container
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    // Header
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(header), 10);
    
    GtkWidget *title_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title_label), 
        "<span size='x-large' weight='bold'>TEACHER DASHBOARD</span>");
    gtk_box_pack_start(GTK_BOX(header), title_label, TRUE, FALSE, 0);
    
    GtkWidget *logout_btn = gtk_button_new_with_label("Logout");
    gtk_widget_set_size_request(logout_btn, 100, 35);
    g_signal_connect(logout_btn, "clicked", G_CALLBACK(on_logout_clicked), &data);
    gtk_box_pack_end(GTK_BOX(header), logout_btn, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
    
    // Notebook with tabs
    data.notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(data.notebook), GTK_POS_TOP);
    
    // Create tabs using component functions
    GtkWidget *quiz_tab = createQuizTab(sock);
    GtkWidget *question_tab = createQuestionTab(sock);
    GtkWidget *bank_tab = createBankTab(sock);
    GtkWidget *results_tab = createResultsTab(sock);
    
    // Append tabs to notebook
    gtk_notebook_append_page(GTK_NOTEBOOK(data.notebook), quiz_tab, 
        gtk_label_new("📋 Quản lý bài thi"));
    gtk_notebook_append_page(GTK_NOTEBOOK(data.notebook), question_tab, 
        gtk_label_new("❓ Quản lý câu hỏi"));
    gtk_notebook_append_page(GTK_NOTEBOOK(data.notebook), bank_tab, 
        gtk_label_new("🏦 Ngân hàng câu hỏi"));
    gtk_notebook_append_page(GTK_NOTEBOOK(data.notebook), results_tab, 
        gtk_label_new("📊 Kết quả thi"));
    
    gtk_box_pack_start(GTK_BOX(vbox), data.notebook, TRUE, TRUE, 0);
    
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    // Signals
    g_signal_connect_swapped(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    gtk_widget_show_all(window);
    gtk_main();
    
    std::cout << "[Teacher Dashboard] Closed, shouldLogout=" << data.shouldLogout << std::endl;
}
