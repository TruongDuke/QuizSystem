#include "../include/quiz_interface.h"
#include "../include/client.h"
#include <iostream>
#include <string>
#include <limits>

// ==================================================
// Teacher: Manage Questions Submenu
// ==================================================

void manageQuestionsMenu(int sock, int quizId) {
    while (true) {
        int choice;

        std::cout << "\n=========== MANAGE QUESTIONS ===========\n";
        std::cout << "Quiz ID: " << quizId << "\n";
        std::cout << "1. View questions\n";
        std::cout << "2. Add question (manual)\n";
        std::cout << "3. Add question from question bank\n";
        std::cout << "4. Edit question\n";
        std::cout << "5. Delete question\n";
        std::cout << "6. Back\n";
        std::cout << "========================================\n";
        std::cout << "Enter choice: ";
        std::cin >> choice;

        if (choice == 1) {
            sendLine(sock, "LIST_QUESTIONS|" + std::to_string(quizId));
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 2) {
            std::string q, o1, o2, o3, o4;
            int correct;

            std::cin.ignore();
            std::cout << "Enter question text: ";
            getline(std::cin, q);

            std::cout << "Option 1: "; getline(std::cin, o1);
            std::cout << "Option 2: "; getline(std::cin, o2);
            std::cout << "Option 3: "; getline(std::cin, o3);
            std::cout << "Option 4: "; getline(std::cin, o4);

            std::cout << "Correct option (1-4): ";
            std::cin >> correct;

            std::string msg =
                "ADD_QUESTION|" + std::to_string(quizId) + "|" + q + "|" +
                o1 + "|" + o2 + "|" + o3 + "|" + o4 + "|" +
                std::to_string(correct);

            sendLine(sock, msg);
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 3) {
            // Xem ngân hàng câu hỏi với bộ lọc
            std::string topic, difficulty;
            std::cout << "\n--- Filter Options ---\n";
            std::cin.ignore();
            std::cout << "Filter by topic (or press Enter for all): ";
            getline(std::cin, topic);
            
            std::cout << "Filter by difficulty (easy/medium/hard/all, or press Enter for all): ";
            getline(std::cin, difficulty);
            
            // Chuẩn hóa input
            if (topic.empty()) topic = "all";
            if (difficulty.empty()) difficulty = "all";
            
            std::string msg = "LIST_QUESTION_BANK|" + topic + "|" + difficulty;
            sendLine(sock, msg);
            
            std::string response = recvLine(sock);
            std::cout << "\nQuestion Bank:\n" << response << std::endl;
            
            // Chọn câu hỏi để thêm
            int bankQId;
            std::cout << "\nEnter bank question ID to add (0 to cancel): ";
            std::cin >> bankQId;
            
            if (bankQId > 0) {
                std::string msg = "ADD_TO_QUIZ_FROM_BANK|" + 
                                  std::to_string(quizId) + "|" + 
                                  std::to_string(bankQId);
                sendLine(sock, msg);
                std::cout << "Server: " << recvLine(sock) << std::endl;
            }

        } else if (choice == 4) {
            int qid, correct;
            std::string q, o1, o2, o3, o4;

            std::cout << "Enter question ID to edit: ";
            std::cin >> qid;
            std::cin.ignore();

            std::cout << "New text: "; getline(std::cin, q);
            std::cout << "New opt1: "; getline(std::cin, o1);
            std::cout << "New opt2: "; getline(std::cin, o2);
            std::cout << "New opt3: "; getline(std::cin, o3);
            std::cout << "New opt4: "; getline(std::cin, o4);

            std::cout << "Correct option: ";
            std::cin >> correct;

            std::string msg =
                "EDIT_QUESTION|" + std::to_string(qid) + "|" + q + "|" +
                o1 + "|" + o2 + "|" + o3 + "|" + o4 + "|" +
                std::to_string(correct);

            sendLine(sock, msg);
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 5) {
            int qid;
            std::cout << "Enter question ID: ";
            std::cin >> qid;

            sendLine(sock, "DELETE_QUESTION|" + std::to_string(qid));
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 6) {
            break;
        }
    }
}

// ==================================================
// Teacher Menu (FULL VERSION)
// ==================================================

void teacherMenu(int sock) {
    while (true) {
        int choice;

        std::cout << "\n====================================\n";
        std::cout << "           TEACHER MENU\n";
        std::cout << "====================================\n";
        std::cout << "1. View all quizzes\n";
        std::cout << "2. Add new quiz\n";
        std::cout << "3. Edit quiz\n";
        std::cout << "4. Delete quiz\n";
        std::cout << "5. Manage questions of a quiz\n";
        std::cout << "6. Manage question bank\n";
        std::cout << "7. View exam results\n";
        std::cout << "8. Logout\n";
        std::cout << "====================================\n";
        std::cout << "Enter your choice: ";
        std::cin >> choice;

        if (choice == 1) {
            sendLine(sock, "LIST_QUIZZES");
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 2) {
            std::string title, desc;
            int count, timeLimit;

            std::cin.ignore();
            std::cout << "Enter quiz title: ";
            getline(std::cin, title);

            std::cout << "Enter description: ";
            getline(std::cin, desc);

            std::cout << "Enter question count: ";
            std::cin >> count;

            std::cout << "Enter time limit (seconds): ";
            std::cin >> timeLimit;

            std::cout << "Exam type (1=Normal, 2=Scheduled): ";
            int typeChoice;
            std::cin >> typeChoice;

            std::string msg;
            if (typeChoice == 2) {
                // Scheduled exam
                std::string startTime, endTime;
                std::cin.ignore();
                std::cout << "Enter start time (YYYY-MM-DD HH:MM:SS): ";
                getline(std::cin, startTime);
                std::cout << "Enter end time (YYYY-MM-DD HH:MM:SS): ";
                getline(std::cin, endTime);
                
                msg = "ADD_QUIZ|" + title + "|" + desc + "|" +
                      std::to_string(count) + "|" + std::to_string(timeLimit) + "|" +
                      "scheduled|" + startTime + "|" + endTime;
            } else {
                // Normal exam
                msg = "ADD_QUIZ|" + title + "|" + desc + "|" +
                      std::to_string(count) + "|" + std::to_string(timeLimit) + "|normal";
            }

            sendLine(sock, msg);
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 3) {
            int quizId;
            std::string title, desc;
            int count, timeLimit;

            std::cout << "Enter quiz ID to edit: ";
            std::cin >> quizId;

            std::cin.ignore();
            std::cout << "New title: ";
            getline(std::cin, title);

            std::cout << "New description: ";
            getline(std::cin, desc);

            std::cout << "New question count: ";
            std::cin >> count;

            std::cout << "New time limit: ";
            std::cin >> timeLimit;

            std::string msg =
                "EDIT_QUIZ|" + std::to_string(quizId) + "|" +
                title + "|" + desc + "|" +
                std::to_string(count) + "|" +
                std::to_string(timeLimit);

            sendLine(sock, msg);
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 4) {
            int quizId;
            std::cout << "Enter quiz ID to delete: ";
            std::cin >> quizId;

            sendLine(sock, "DELETE_QUIZ|" + std::to_string(quizId));
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 5) {
            int quizId;
            std::cout << "Enter quiz ID to manage questions: ";
            std::cin >> quizId;

            manageQuestionsMenu(sock, quizId);

        } else if (choice == 6) {
            manageQuestionBankMenu(sock);

        } else if (choice == 7) {
            sendLine(sock, "LIST_EXAMS");
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 8) {
            sendLine(sock, "QUIT");
            std::cout << "Logging out...\n";
            break;

        } else {
            std::cout << "Invalid choice.\n";
        }
    }
}

// ==================================================
// Teacher: Manage Question Bank Menu
// ==================================================

void manageQuestionBankMenu(int sock) {
    while (true) {
        int choice;
        std::cout << "\n=========== QUESTION BANK ===========\n";
        std::cout << "1. View all questions in bank\n";
        std::cout << "2. View questions by topic\n";
        std::cout << "3. View questions by difficulty\n";
        std::cout << "4. View questions by topic and difficulty\n";
        std::cout << "5. Add question to bank\n";
        std::cout << "6. View question details\n";
        std::cout << "7. Back\n";
        std::cout << "=====================================\n";
        std::cout << "Enter choice: ";
        std::cin >> choice;

        if (choice == 1) {
            sendLine(sock, "LIST_QUESTION_BANK|all|all");
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 2) {
            std::string topic;
            std::cin.ignore();
            std::cout << "Enter topic: ";
            getline(std::cin, topic);
            sendLine(sock, "LIST_QUESTION_BANK|" + topic + "|all");
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 3) {
            std::string difficulty;
            std::cin.ignore();
            std::cout << "Enter difficulty (easy/medium/hard): ";
            getline(std::cin, difficulty);
            
            // Validate difficulty
            if (difficulty != "easy" && difficulty != "medium" && difficulty != "hard") {
                std::cout << "Invalid difficulty. Use: easy, medium, or hard\n";
                continue;
            }
            
            sendLine(sock, "LIST_QUESTION_BANK|all|" + difficulty);
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 4) {
            std::string topic, difficulty;
            std::cin.ignore();
            std::cout << "Enter topic: ";
            getline(std::cin, topic);
            std::cout << "Enter difficulty (easy/medium/hard): ";
            getline(std::cin, difficulty);
            
            // Validate difficulty
            if (difficulty != "easy" && difficulty != "medium" && difficulty != "hard") {
                std::cout << "Invalid difficulty. Use: easy, medium, or hard\n";
                continue;
            }
            
            sendLine(sock, "LIST_QUESTION_BANK|" + topic + "|" + difficulty);
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 5) {
            std::string q, o1, o2, o3, o4, diff, topic;
            int correct;
            std::cin.ignore();
            std::cout << "Enter question text: ";
            getline(std::cin, q);
            std::cout << "Option 1: "; getline(std::cin, o1);
            std::cout << "Option 2: "; getline(std::cin, o2);
            std::cout << "Option 3: "; getline(std::cin, o3);
            std::cout << "Option 4: "; getline(std::cin, o4);
            std::cout << "Correct option (1-4): ";
            std::cin >> correct;
            std::cin.ignore();
            
            // Validate và nhập difficulty
            while (true) {
                std::cout << "Difficulty (easy/medium/hard): ";
                getline(std::cin, diff);
                if (diff == "easy" || diff == "medium" || diff == "hard") {
                    break;
                }
                std::cout << "Invalid! Please enter: easy, medium, or hard\n";
            }
            
            std::cout << "Topic: ";
            getline(std::cin, topic);
            
            std::string msg = "ADD_TO_BANK|" + q + "|" + o1 + "|" + o2 + "|" + 
                         o3 + "|" + o4 + "|" + std::to_string(correct) + "|" + 
                         diff + "|" + topic;
            sendLine(sock, msg);
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 6) {
            int bankQId;
            std::cout << "Enter bank question ID: ";
            std::cin >> bankQId;
            sendLine(sock, "GET_QUESTION_BANK|" + std::to_string(bankQId));
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 7) {
            break;
        }
    }
}

