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
        std::cout << "2. Add question\n";
        std::cout << "3. Edit question\n";
        std::cout << "4. Delete question\n";
        std::cout << "5. Back\n";
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

        } else if (choice == 4) {
            int qid;
            std::cout << "Enter question ID: ";
            std::cin >> qid;

            sendLine(sock, "DELETE_QUESTION|" + std::to_string(qid));
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 5) {
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
        std::cout << "6. View exam results\n";
        std::cout << "7. Logout\n";
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

            std::cout << "Enter time limit: ";
            std::cin >> timeLimit;

            std::string msg =
                "ADD_QUIZ|" + title + "|" + desc + "|" +
                std::to_string(count) + "|" + std::to_string(timeLimit);

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
            sendLine(sock, "LIST_EXAMS");
            std::cout << "Server: " << recvLine(sock) << std::endl;

        } else if (choice == 7) {
            sendLine(sock, "QUIT");
            std::cout << "Logging out...\n";
            break;

        } else {
            std::cout << "Invalid choice.\n";
        }
    }
}

