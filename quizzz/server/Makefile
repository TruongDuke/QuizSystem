# Makefile for Quiz Server

# Compiler
CXX = g++

# MySQL Connector/C++ path
MYSQL_CPP_DIR = /opt/homebrew/opt/mysql-connector-c++

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build

# Source files
SOURCES = $(SRC_DIR)/server.cpp \
          $(SRC_DIR)/db_manager.cpp \
          $(SRC_DIR)/protocol_manager.cpp \
          $(SRC_DIR)/session_manager.cpp \
          $(SRC_DIR)/quiz_manager.cpp \
          $(SRC_DIR)/question_manager.cpp

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Target executable
TARGET = quiz_server

# Compiler flags
CXXFLAGS = -std=c++11 -Wall -Wextra
INCLUDES = -I$(INCLUDE_DIR) -I$(MYSQL_CPP_DIR)/include
LIBS = -L$(MYSQL_CPP_DIR)/lib -lmysqlcppconn

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LIBS)

# Compile source files to object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Rebuild
rebuild: clean all

.PHONY: all clean rebuild

