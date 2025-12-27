# Makefile for Quiz Client

# Compiler
CXX = g++

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build

# Source files for console version
SOURCES = $(SRC_DIR)/client.cpp \
          $(SRC_DIR)/quiz_interface.cpp \
          $(SRC_DIR)/question_interface.cpp

# Source files for GUI version (sử dụng network_utils.cpp thay vì client.cpp)
SOURCES_GUI = $(SRC_DIR)/client_gui.cpp \
              $(SRC_DIR)/network_utils.cpp \
              $(SRC_DIR)/quiz_interface.cpp \
              $(SRC_DIR)/question_interface.cpp \
              $(SRC_DIR)/gui/gui_login.cpp \
              $(SRC_DIR)/gui/gui_room_list.cpp \
              $(SRC_DIR)/gui/gui_exam.cpp \
              $(SRC_DIR)/gui/gui_history.cpp

# Object files
OBJECTS = $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)
OBJECTS_GUI = $(SOURCES_GUI:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# Target executables
TARGET = quiz_client
TARGET_GUI = quiz_client_gui

# Compiler flags
CXXFLAGS = -std=c++11 -Wall -Wextra
INCLUDES = -I$(INCLUDE_DIR)

# GTK flags
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)

# Default target - build cả 2 version
all: $(BUILD_DIR) $(TARGET) $(TARGET_GUI)

# Build console version only
console: $(BUILD_DIR) $(TARGET)

# Build GUI version only
gui: $(BUILD_DIR) $(TARGET_GUI)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/gui

# Link object files to create console executable
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET)

# Link object files to create GUI executable
$(TARGET_GUI): $(OBJECTS_GUI)
	$(CXX) $(OBJECTS_GUI) -o $(TARGET_GUI) $(GTK_LIBS)

# Compile source files to object files (console)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Compile GUI source files with GTK flags
$(BUILD_DIR)/gui/%.o: $(SRC_DIR)/gui/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(GTK_CFLAGS) -c $< -o $@

# Special rule for client_gui.cpp
$(BUILD_DIR)/client_gui.o: $(SRC_DIR)/client_gui.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(GTK_CFLAGS) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(TARGET_GUI)

# Rebuild
rebuild: clean all

.PHONY: all console gui clean rebuild

