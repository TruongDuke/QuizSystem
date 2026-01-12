#include "../../include/gui/gui_login.h"
#include <cstring>

// Cấu trúc dữ liệu cho callbacks
struct LoginData {
    GtkWidget *window;
    GtkWidget *login_username_entry;
    GtkWidget *login_password_entry;
    GtkWidget *reg_username_entry;
    GtkWidget *reg_password_entry;
    GtkWidget *email_entry;
    GtkWidget *role_combo;
    GtkWidget *stack;
    LoginResult *result;
};

// Callback khi nhấn nút Login
static void on_login_clicked(GtkWidget *widget, gpointer data) {
    LoginData *login_data = (LoginData*)data;
    
    const char *username = gtk_entry_get_text(GTK_ENTRY(login_data->login_username_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(login_data->login_password_entry));
    
    if (strlen(username) == 0 || strlen(password) == 0) {
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(login_data->window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_OK,
            "Vui lòng nhập đầy đủ username và password!"
        );
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        return;
    }
    
    login_data->result->success = true;
    login_data->result->username = username;
    login_data->result->password = password;
    login_data->result->isRegister = false;
    
    gtk_main_quit();
}

// Callback khi nhấn nút Register
static void on_register_clicked(GtkWidget *widget, gpointer data) {
    LoginData *login_data = (LoginData*)data;
    
    const char *username = gtk_entry_get_text(GTK_ENTRY(login_data->reg_username_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(login_data->reg_password_entry));
    const char *email = gtk_entry_get_text(GTK_ENTRY(login_data->email_entry));
    
    gchar *role_text = gtk_combo_box_text_get_active_text(
        GTK_COMBO_BOX_TEXT(login_data->role_combo)
    );
    
    if (strlen(username) == 0 || strlen(password) == 0 || 
        strlen(email) == 0 || role_text == NULL) {
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(login_data->window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_OK,
            "Vui lòng nhập đầy đủ thông tin!"
        );
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        if (role_text) g_free(role_text);
        return;
    }
    
    login_data->result->success = true;
    login_data->result->username = username;
    login_data->result->password = password;
    login_data->result->email = email;
    login_data->result->role = role_text;
    login_data->result->isRegister = true;
    
    g_free(role_text);
    gtk_main_quit();
}

// Callback khi nhấn nút chuyển sang Register
static void on_switch_to_register(GtkWidget *widget, gpointer data) {
    LoginData *login_data = (LoginData*)data;
    gtk_stack_set_visible_child_name(GTK_STACK(login_data->stack), "register");
}

// Callback khi nhấn nút chuyển sang Login
static void on_switch_to_login(GtkWidget *widget, gpointer data) {
    LoginData *login_data = (LoginData*)data;
    gtk_stack_set_visible_child_name(GTK_STACK(login_data->stack), "login");
}

// Tạo trang Login
static GtkWidget* create_login_page(LoginData *login_data) {
    // Container chính
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 30);
    gtk_widget_set_margin_end(box, 30);
    gtk_widget_set_margin_top(box, 30);
    gtk_widget_set_margin_bottom(box, 30);
    
    // Tiêu đề
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), 
        "<span font='20' weight='bold'>QUIZ SYSTEM - LOGIN</span>");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 10);
    
    // Username
    GtkWidget *username_label = gtk_label_new("Username:");
    gtk_widget_set_halign(username_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), username_label, FALSE, FALSE, 5);
    
    login_data->login_username_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(login_data->login_username_entry), "Nhập username");
    gtk_box_pack_start(GTK_BOX(box), login_data->login_username_entry, FALSE, FALSE, 0);
    
    // Password
    GtkWidget *password_label = gtk_label_new("Password:");
    gtk_widget_set_halign(password_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), password_label, FALSE, FALSE, 5);
    
    login_data->login_password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(login_data->login_password_entry), "Nhập password");
    gtk_entry_set_visibility(GTK_ENTRY(login_data->login_password_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(box), login_data->login_password_entry, FALSE, FALSE, 0);
    
    // Nút Login
    GtkWidget *login_button = gtk_button_new_with_label("LOGIN");
    gtk_widget_set_size_request(login_button, -1, 40);
    g_signal_connect(login_button, "clicked", G_CALLBACK(on_login_clicked), login_data);
    gtk_box_pack_start(GTK_BOX(box), login_button, FALSE, FALSE, 10);
    
    // Link chuyển sang Register
    GtkWidget *register_link = gtk_button_new_with_label("Chưa có tài khoản? Đăng ký ngay");
    g_signal_connect(register_link, "clicked", G_CALLBACK(on_switch_to_register), login_data);
    gtk_box_pack_start(GTK_BOX(box), register_link, FALSE, FALSE, 5);
    
    return box;
}

// Tạo trang Register
static GtkWidget* create_register_page(LoginData *login_data) {
    // Container chính
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 30);
    gtk_widget_set_margin_end(box, 30);
    gtk_widget_set_margin_top(box, 30);
    gtk_widget_set_margin_bottom(box, 30);
    
    // Tiêu đề
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), 
        "<span font='20' weight='bold'>QUIZ SYSTEM - REGISTER</span>");
    gtk_box_pack_start(GTK_BOX(box), title, FALSE, FALSE, 10);
    
    // Username
    GtkWidget *username_label = gtk_label_new("Username:");
    gtk_widget_set_halign(username_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), username_label, FALSE, FALSE, 5);
    
    login_data->reg_username_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(login_data->reg_username_entry), "Nhập username");
    gtk_box_pack_start(GTK_BOX(box), login_data->reg_username_entry, FALSE, FALSE, 0);
    
    // Password
    GtkWidget *password_label = gtk_label_new("Password:");
    gtk_widget_set_halign(password_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), password_label, FALSE, FALSE, 5);
    
    login_data->reg_password_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(login_data->reg_password_entry), "Nhập password");
    gtk_entry_set_visibility(GTK_ENTRY(login_data->reg_password_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(box), login_data->reg_password_entry, FALSE, FALSE, 0);
    
    // Email
    GtkWidget *email_label = gtk_label_new("Email:");
    gtk_widget_set_halign(email_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), email_label, FALSE, FALSE, 5);
    
    login_data->email_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(login_data->email_entry), "Nhập email");
    gtk_box_pack_start(GTK_BOX(box), login_data->email_entry, FALSE, FALSE, 0);
    
    // Role
    GtkWidget *role_label = gtk_label_new("Role:");
    gtk_widget_set_halign(role_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), role_label, FALSE, FALSE, 5);
    
    login_data->role_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(login_data->role_combo), "student");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(login_data->role_combo), "teacher");
    gtk_combo_box_set_active(GTK_COMBO_BOX(login_data->role_combo), 0);
    gtk_box_pack_start(GTK_BOX(box), login_data->role_combo, FALSE, FALSE, 0);
    
    // Nút Register
    GtkWidget *register_button = gtk_button_new_with_label("REGISTER");
    gtk_widget_set_size_request(register_button, -1, 40);
    g_signal_connect(register_button, "clicked", G_CALLBACK(on_register_clicked), login_data);
    gtk_box_pack_start(GTK_BOX(box), register_button, FALSE, FALSE, 10);
    
    // Link chuyển về Login
    GtkWidget *login_link = gtk_button_new_with_label("Đã có tài khoản? Đăng nhập");
    g_signal_connect(login_link, "clicked", G_CALLBACK(on_switch_to_login), login_data);
    gtk_box_pack_start(GTK_BOX(box), login_link, FALSE, FALSE, 5);
    
    return box;
}

// Hàm chính hiển thị cửa sổ login
LoginResult showLoginWindow() {
    LoginResult result = {false, "", "", false, "", ""};
    
    // Khởi tạo GTK
    gtk_init(NULL, NULL);
    
    // Tạo cửa sổ
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Quiz System - Login");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 500);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    
    // Tạo LoginData
    LoginData login_data;
    login_data.window = window;
    login_data.result = &result;
    
    // Tạo Stack để chuyển đổi giữa Login và Register
    login_data.stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(login_data.stack), 
                                  GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    
    // Thêm các trang
    GtkWidget *login_page = create_login_page(&login_data);
    GtkWidget *register_page = create_register_page(&login_data);
    
    gtk_stack_add_named(GTK_STACK(login_data.stack), login_page, "login");
    gtk_stack_add_named(GTK_STACK(login_data.stack), register_page, "register");
    
    gtk_container_add(GTK_CONTAINER(window), login_data.stack);
    
    // Signal khi đóng cửa sổ
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // Hiển thị
    gtk_widget_show_all(window);
    gtk_main();
    
    // Destroy window trước khi return để không bị chồng
    gtk_widget_destroy(window);
    
    // Process pending events để đảm bảo window bị destroy hoàn toàn
    while (gtk_events_pending())
        gtk_main_iteration();
    
    return result;
}
