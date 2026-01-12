#ifndef GUI_ROOM_LIST_H
#define GUI_ROOM_LIST_H

#include <gtk/gtk.h>
#include <string>
#include <vector>

// Forward declaration
void showHistoryWindow(int sock);

// Thông tin phòng thi
struct ExamRoom {
    int examId;
    std::string title;
    std::string description;
    int questionCount;
    int timeLimit;
    std::string status;
};

// Kết quả từ GUI Room List
struct RoomListResult {
    bool selectedRoom;
    int examId;
    std::string title;
    int questionCount;
};

// Hiển thị danh sách phòng thi
RoomListResult showRoomListWindow(int sock);

#endif // GUI_ROOM_LIST_H
