#ifndef GUI_LOGIN_H
#define GUI_LOGIN_H

#include <gtk/gtk.h>
#include <string>

// Kết quả từ GUI Login
struct LoginResult {
    bool success;
    std::string username;
    std::string password;
    bool isRegister;
    std::string email;
    std::string role;
};

// Hiển thị màn hình Login/Register và trả về kết quả
LoginResult showLoginWindow();

#endif // GUI_LOGIN_H
