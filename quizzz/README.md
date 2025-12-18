# Quiz System - Hệ thống thi trắc nghiệm

## Cấu trúc dự án

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
│   │   └── quiz_interface.cpp  # Giao diện quiz cho giáo viên
│   ├── include/
│   │   ├── client.h            # Header cho client.cpp
│   │   └── quiz_interface.h     # Header cho quiz_interface.cpp
│   └── Makefile                # Cấu hình biên dịch
│
└── database/
    └── quiz.sql                # Script tạo database
```

## Yêu cầu hệ thống

- **MySQL Server** (đã cài đặt và đang chạy)
- **MySQL Connector/C++** (cài qua Homebrew: `brew install mysql-connector-c++`)
- **G++ compiler** với C++11 support
- **Make** utility

## Cài đặt

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

## Chạy ứng dụng

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

## Cấu hình

### Database Connection

Mặc định server kết nối với:
- Host: `127.0.0.1`
- User: `root`
- Password: `123456`
- Database: `quizDB`

Để thay đổi, sửa trong `server/src/server.cpp` dòng 527.

### MySQL Connector Path

Nếu MySQL Connector/C++ được cài ở đường dẫn khác, sửa trong `server/Makefile`:
```makefile
MYSQL_CPP_DIR = /your/path/to/mysql-connector-c++
```

## Module mô tả

### Server Side

- **protocol_manager**: Xử lý giao tiếp mạng (sendLine, recvLine) và utilities (split, escapeSql)
- **session_manager**: Quản lý session của người dùng
- **db_manager**: Quản lý kết nối và thao tác với MySQL database
- **quiz_manager**: Xử lý các thao tác liên quan đến quiz (list, add, edit, delete)
- **question_manager**: Xử lý các thao tác liên quan đến câu hỏi (list, add, edit, delete)
- **server**: Main server logic, routing commands, và xử lý login

### Client Side

- **client**: Kết nối server, xử lý login, và routing theo role
- **quiz_interface**: Giao diện menu cho giáo viên (quản lý quiz và câu hỏi)

## Lệnh Make

### Server
- `make` - Biên dịch server
- `make clean` - Xóa các file build
- `make rebuild` - Clean và build lại

### Client
- `make` - Biên dịch client
- `make clean` - Xóa các file build
- `make rebuild` - Clean và build lại

## Giao thức

Server và client giao tiếp qua socket với format:
- Mỗi message kết thúc bằng `\n`
- Format: `COMMAND|param1|param2|...`

### Các lệnh hỗ trợ:
- `LOGIN|username|password`
- `LIST_QUIZZES`
- `ADD_QUIZ|title|desc|count|time`
- `EDIT_QUIZ|quizId|title|desc|count|time`
- `DELETE_QUIZ|quizId`
- `LIST_QUESTIONS|quizId`
- `ADD_QUESTION|quizId|content|opt1|opt2|opt3|opt4|correctIndex`
- `EDIT_QUESTION|qId|content|opt1|opt2|opt3|opt4|correctIndex`
- `DELETE_QUESTION|qId`
- `LIST_EXAMS`
- `QUIT`

## Tài khoản mẫu

Database đã có sẵn các tài khoản:
- **Admin**: username=`admin`, password=`admin_password`
- **Teacher**: username=`teacher1`, password=`teacher_password`
- **Student**: username=`student1`, password=`student_password`

