# Quiz System - Hệ thống thi trắc nghiệm

Hệ thống thi trắc nghiệm với kiến trúc module, hỗ trợ Teacher quản lý quiz và Student làm bài thi.

## 📁 Cấu trúc dự án

```
quiz_app/
│
├── server/                     # Mã nguồn phía server
│   ├── src/
│   │   ├── db_manager.cpp      # Quản lý kết nối DB
│   │   ├── server.cpp          # Logic chính server
│   │   ├── quiz_manager.cpp    # Quản lý quiz
│   │   ├── question_manager.cpp # Quản lý câu hỏi
│   │   ├── session_manager.cpp  # Quản lý session
│   │   └── protocol_manager.cpp # Quản lý giao thức
│   ├── include/
│   │   ├── db_manager.h        # Header cho db_manager.cpp
│   │   ├── server.h            # Header cho server.cpp
│   │   ├── quiz_manager.h      # Header cho quiz_manager.cpp
│   │   ├── question_manager.h  # Header cho question_manager.cpp
│   │   ├── session_manager.h   # Header cho session_manager.cpp
│   │   └── protocol_manager.h  # Header cho protocol_manager.cpp
│   └── Makefile                # Cấu hình biên dịch
│
├── client/                     # Mã nguồn phía client
│   ├── src/
│   │   ├── client.cpp          # Logic chính client
│   │   ├── quiz_interface.cpp  # Giao diện quiz cho giáo viên
│   │   └── question_interface.cpp  # Giao diện câu hỏi cho học sinh
│   ├── include/
│   │   ├── client.h            # Header cho client.cpp
│   │   ├── quiz_interface.h    # Header cho quiz_interface.cpp
│   │   └── question_interface.h  # Header cho question_interface.cpp
│   └── Makefile                # Cấu hình biên dịch
│
└── database/
    └── quiz.sql                # Script tạo database
```

## 🔧 Yêu cầu hệ thống

- **MySQL Server** (đã cài đặt và đang chạy)
- **MySQL Connector/C++** (cài qua Homebrew: `brew install mysql-connector-c++`)
- **G++ compiler** với C++11 support
- **Make** utility

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

Server sẽ chạy trên port **9000**.

### 2. Chạy Client (terminal khác)

```bash
cd client
./quiz_client
```

## ⚙️ Cấu hình

### Database Connection

Mặc định server kết nối với:
- Host: `127.0.0.1`
- User: `root`
- Password: `123456`
- Database: `quizDB`

Để thay đổi, sửa trong `server/src/server.cpp` dòng 240.

### MySQL Connector Path

Nếu MySQL Connector/C++ được cài ở đường dẫn khác, sửa trong `server/Makefile`:
```makefile
MYSQL_CPP_DIR = /your/path/to/mysql-connector-c++
```

## 📦 Module mô tả

### Server Side

#### Core Modules:
- **`protocol_manager`**: Xử lý giao tiếp mạng (sendLine, recvLine) và utilities (split, escapeSql)
- **`db_manager`**: Quản lý kết nối và thao tác với MySQL database
- **`session_manager`**: Quản lý session của học sinh khi làm bài (StudentSession)

#### Business Logic:
- **`quiz_manager`**: Xử lý các thao tác liên quan đến quiz (list, add, edit, delete)
- **`question_manager`**: Xử lý các thao tác liên quan đến câu hỏi (list, add, edit, delete)
- **`server`**: Main server logic, routing commands, xử lý login, và student exam flow

### Client Side

- **`client`**: Kết nối server, xử lý login, và routing theo role
- **`quiz_interface`**: Giao diện menu cho giáo viên (quản lý quiz và câu hỏi)
- **`question_interface`**: Giao diện menu cho học sinh (tham gia thi, xem lịch sử)

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
- `ADD_QUIZ|title|desc|count|time` - Thêm quiz mới
- `EDIT_QUIZ|quizId|title|desc|count|time` - Sửa quiz
- `DELETE_QUIZ|quizId` - Xóa quiz

#### Question Management (Teacher):
- `LIST_QUESTIONS|quizId` - Liệt kê câu hỏi của quiz
- `ADD_QUESTION|quizId|content|opt1|opt2|opt3|opt4|correctIndex` - Thêm câu hỏi
- `EDIT_QUESTION|qId|content|opt1|opt2|opt3|opt4|correctIndex` - Sửa câu hỏi
- `DELETE_QUESTION|qId` - Xóa câu hỏi

#### Exam Management:
- `LIST_EXAMS` - Liệt kê các bài thi (Teacher)

#### Student Commands:
- `LIST_QUIZZES` - Xem danh sách quiz có sẵn
- `JOIN_ROOM|quizId` - Tham gia làm bài thi
- `ANSWER|qId|choice` - Trả lời câu hỏi (A/B/C/D)
- `LIST_MY_HISTORY` - Xem lịch sử thi của mình

#### Server Responses:
- `QUESTION|qId|text|A|B|C|D` - Gửi câu hỏi cho student
- `TEST_STARTED` - Báo hiệu bắt đầu thi
- `END_EXAM|score` - Kết thúc bài thi với điểm số

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

### Teacher Flow:
1. Login với teacher account
2. Tạo quiz mới (ADD_QUIZ)
3. Thêm câu hỏi vào quiz (ADD_QUESTION)
4. Xem kết quả thi của students (LIST_EXAMS)

### Student Flow:
1. Login với student account
2. Xem danh sách quiz (LIST_QUIZZES)
3. Tham gia thi (JOIN_ROOM|quizId)
4. Nhận câu hỏi và trả lời (ANSWER)
5. Xem điểm và lịch sử (LIST_MY_HISTORY)

## 📝 Ghi chú

- Server xử lý từng client một (single-threaded, blocking I/O)
- Session được quản lý trong memory (StudentSession)
- Mỗi student có thể làm một bài thi tại một thời điểm

## 🚧 Có thể mở rộng

- [ ] Multi-threading cho nhiều clients đồng thời
- [ ] Async I/O với select()/epoll
- [ ] Real-time notifications
- [ ] Timer/countdown cho exam
- [ ] WebSocket support cho web client
- [ ] Database session storage

## 📄 License

Dự án học tập - Educational Project

---

**Tác giả**: Quiz System Team  
**Phiên bản**: 1.0 (Module Architecture)

