# Quiz System - Hệ thống thi trắc nghiệm

Hệ thống thi trắc nghiệm với **real-time communication**, hỗ trợ nhiều clients đồng thời sử dụng **async I/O** với `select()`.

## ✨ Tính năng chính

- ✅ **Multiple Concurrent Clients** - Server xử lý nhiều clients đồng thời
- ✅ **Async I/O với select()** - Non-blocking, không bị treo khi chờ client
- ✅ **Real-time Communication** - Giao tiếp thời gian thực giữa server và clients
- ✅ **Broadcast Messages** - Gửi thông báo cho nhiều clients cùng lúc
- ✅ **Role-based Broadcast** - Gửi thông báo cho nhóm role cụ thể (teacher/student)
- ✅ **Real-time Notifications** - Students nhận thông báo ngay khi teacher tạo quiz mới
- ✅ **Session Management** - Quản lý session cho từng client
- ✅ **Role-based Access Control** - Phân quyền theo role (admin/teacher/student)

## 📁 Cấu trúc dự án

```
quizzz/
│
├── server/                     # Mã nguồn phía server
│   ├── src/
│   │   ├── db_manager.cpp      # Quản lý kết nối DB
│   │   ├── server.cpp          # Logic chính server (async I/O)
│   │   ├── quiz_manager.cpp    # Quản lý quiz
│   │   ├── question_manager.cpp # Quản lý câu hỏi
│   │   ├── session_manager.cpp  # Quản lý session
│   │   ├── protocol_manager.cpp # Quản lý giao thức
│   │   ├── client_manager.cpp   # Quản lý nhiều clients
│   │   └── broadcast_manager.cpp # Broadcast messages
│   ├── include/
│   │   ├── db_manager.h
│   │   ├── server.h
│   │   ├── quiz_manager.h
│   │   ├── question_manager.h
│   │   ├── session_manager.h
│   │   ├── protocol_manager.h
│   │   ├── client_manager.h
│   │   └── broadcast_manager.h
│   └── Makefile
│
├── client/                     # Mã nguồn phía client
│   ├── src/
│   │   ├── client.cpp          # Logic chính client
│   │   └── quiz_interface.cpp  # Giao diện quiz cho giáo viên
│   ├── include/
│   │   ├── client.h
│   │   └── quiz_interface.h
│   └── Makefile
│
└── database/
    └── quiz.sql                # Script tạo database
```

## 🔧 Yêu cầu hệ thống

- **MySQL Server** (đã cài đặt và đang chạy)
- **MySQL Connector/C++** (cài qua Homebrew: `brew install mysql-connector-c++`)
- **G++ compiler** với C++11 support
- **Make** utility
- **macOS/Linux** (đã test trên macOS)

## 🚀 Cài đặt

### 1. Khởi tạo Database

```bash
mysql -u root -p < database/quiz.sql
```

### 2. Biên dịch Server

```bash
cd server
make
```

### 3. Biên dịch Client

```bash
cd client
make
```

## 💻 Chạy ứng dụng

### 1. Chạy Server

```bash
cd server
./quiz_server
```

Server sẽ:
- Chạy trên port **9000**
- Hiển thị: `[NET] Using async I/O with select() - Multiple clients supported`
- Chấp nhận nhiều clients đồng thời

### 2. Chạy Client (có thể chạy nhiều clients)

Mở nhiều terminal và chạy:

```bash
cd client
./quiz_client
```

**Lưu ý:** Bạn có thể mở nhiều terminal và chạy nhiều clients cùng lúc để test tính năng concurrent!

## ⚙️ Cấu hình

### Database Connection

Mặc định server kết nối với:
- Host: `127.0.0.1`
- User: `root`
- Password: `123456`
- Database: `quizDB`

Để thay đổi, sửa trong `server/src/server.cpp` dòng 120.

### MySQL Connector Path

Nếu MySQL Connector/C++ được cài ở đường dẫn khác, sửa trong `server/Makefile`:
```makefile
MYSQL_CPP_DIR = /your/path/to/mysql-connector-c++
```

## 📦 Module mô tả

### Server Side

#### Core Modules:
- **`server`**: Main server logic với async I/O (`select()`), routing commands, xử lý login
- **`protocol_manager`**: Xử lý giao tiếp mạng (sendLine, recvLine) và utilities (split, escapeSql)
- **`db_manager`**: Quản lý kết nối và thao tác với MySQL database

#### Business Logic:
- **`quiz_manager`**: Xử lý các thao tác liên quan đến quiz (list, add, edit, delete)
- **`question_manager`**: Xử lý các thao tác liên quan đến câu hỏi (list, add, edit, delete)

#### Real-time & Management:
- **`session_manager`**: Quản lý session của người dùng (tạo, validate, xóa session)
- **`client_manager`**: Quản lý nhiều clients đồng thời (thêm, xóa, lấy thông tin client)
- **`broadcast_manager`**: Gửi messages cho nhiều clients (broadcast to all, broadcast to role)

### Client Side

- **`client`**: Kết nối server, xử lý login, và routing theo role
- **`quiz_interface`**: Giao diện menu cho giáo viên (quản lý quiz và câu hỏi)

## 🔄 Async I/O với select()

### Cách hoạt động:

Server sử dụng `select()` để:
- **Non-blocking**: Không bị treo khi chờ client (timeout 1 giây)
- **Multiple clients**: Xử lý nhiều clients đồng thời
- **Event-driven**: Chỉ xử lý khi có sự kiện (client kết nối, client gửi data)

### So sánh:

| Tính năng | Blocking (cũ) | Async I/O với select() (mới) |
|----------|---------------|-------------------------------|
| Số client | 1 client | Nhiều clients đồng thời ✅ |
| Chờ đợi | Treo mãi mãi ❌ | Timeout 1 giây ✅ |
| Hiệu suất | Thấp | Cao ✅ |
| Real-time | Không | Có ✅ |

## 📡 Real-time Communication

### Broadcast Messages

Server có thể gửi thông báo cho:
- **Tất cả clients**: `broadcast(message)`
- **Nhóm role cụ thể**: `broadcastToRole("student", message)`
- **Client cụ thể**: `sendToClient(sock, message)`

### Ví dụ Real-time Notification:

```
Teacher: ADD_QUIZ|New Quiz|Description|10|600
Server: ADD_QUIZ_OK|quizId=5
Server: [Broadcast] NOTIFICATION|new_quiz_available → Tất cả students
```

Tất cả students đang online sẽ nhận được thông báo ngay lập tức! 🎉

## 🛠️ Lệnh Make

### Server
```bash
make          # Biên dịch server
make clean    # Xóa các file build
make rebuild  # Clean và build lại
```

### Client
```bash
make          # Biên dịch client
make clean    # Xóa các file build
make rebuild  # Clean và build lại
```

## 📋 Giao thức

Server và client giao tiếp qua socket với format:
- Mỗi message kết thúc bằng `\n`
- Format: `COMMAND|param1|param2|...`

### Các lệnh hỗ trợ:

#### Authentication:
- `LOGIN|username|password` - Đăng nhập

#### Quiz Management (Teacher):
- `LIST_QUIZZES` - Liệt kê tất cả quiz
- `ADD_QUIZ|title|desc|count|time` - Thêm quiz mới (broadcast notification)
- `EDIT_QUIZ|quizId|title|desc|count|time` - Sửa quiz
- `DELETE_QUIZ|quizId` - Xóa quiz

#### Question Management (Teacher):
- `LIST_QUESTIONS|quizId` - Liệt kê câu hỏi của quiz
- `ADD_QUESTION|quizId|content|opt1|opt2|opt3|opt4|correctIndex` - Thêm câu hỏi
- `EDIT_QUESTION|qId|content|opt1|opt2|opt3|opt4|correctIndex` - Sửa câu hỏi
- `DELETE_QUESTION|qId` - Xóa câu hỏi

#### Exam Management:
- `LIST_EXAMS` - Liệt kê các bài thi

#### Notifications:
- `NOTIFICATION|new_quiz_available` - Thông báo quiz mới (tự động gửi)

#### Other:
- `QUIT` - Thoát

## 👥 Tài khoản mẫu

Database đã có sẵn các tài khoản:
- **Admin**: 
  - Username: `admin`
  - Password: `admin_password`
- **Teacher**: 
  - Username: `teacher1`
  - Password: `teacher_password`
- **Student**: 
  - Username: `student1`
  - Password: `student_password`

## 🎯 Use Cases

### 1. Multiple Teachers cùng quản lý quiz
- Nhiều teachers có thể đăng nhập cùng lúc
- Mỗi teacher có thể tạo/sửa quiz độc lập
- Students nhận notification khi có quiz mới

### 2. Real-time Notifications
- Teacher tạo quiz mới → Tất cả students nhận thông báo ngay
- Có thể mở rộng: thông báo khi quiz sắp bắt đầu, kết quả thi, etc.

### 3. Concurrent Access
- Nhiều students có thể xem danh sách quiz cùng lúc
- Không bị blocking khi một client đang xử lý

## 🔍 Kiến trúc

### Async I/O Flow:

```
Server Start
    ↓
Listen on port 9000
    ↓
Loop với select():
    ├─→ New client? → Accept & Add to ClientManager
    ├─→ Client data? → Handle command
    └─→ Timeout? → Continue loop (không blocking)
```

### Broadcast Flow:

```
Teacher: ADD_QUIZ
    ↓
Server: Create quiz in DB
    ↓
Server: Send response to teacher
    ↓
Server: BroadcastManager.broadcastToRole("student", "NOTIFICATION|new_quiz_available")
    ↓
All Students: Receive notification
```

## 📝 Ghi chú

- Server sử dụng `select()` cho async I/O (có thể nâng cấp lên `epoll` trên Linux hoặc `kqueue` trên macOS)
- Session được quản lý trong memory (có thể lưu vào DB để scale)
- Broadcast messages được gửi ngay lập tức (không có queue)

## 🚧 Có thể mở rộng

- [ ] WebSocket support cho web client
- [ ] Message queue cho broadcast (Redis/RabbitMQ)
- [ ] Database session storage
- [ ] Student interface (làm bài thi)
- [ ] Real-time exam monitoring
- [ ] Chat/Announcement system

## 📄 License

Dự án học tập - Educational Project

---

**Tác giả**: Quiz System Team  
**Phiên bản**: 2.0 (với Async I/O & Real-time Communication)



---

## 🖥️ GUI Client (GTK+3)

### Cài đặt GTK+3:
```bash
# Ubuntu/WSL2
sudo apt update && sudo apt install libgtk-3-dev

# macOS
brew install gtk+3
```

### Build và chạy:
```bash
# Terminal 1: Server
cd server && make && ./quiz_server

# Terminal 2: GUI Client
cd client && make gui && ./quiz_client_gui
```

**Tài khoản test**: `student1` / `student_password` hoặc đăng ký tài khoản học sinh, chưa phát triển phần giáo viên nên khi log in bằng giáo viên sẽ ra console

**Chức năng**: Login/Register → Danh sách quiz → Làm bài thi → Xem lịch sử

**Lưu ý**: Console client vẫn hoạt động (`make console`).
