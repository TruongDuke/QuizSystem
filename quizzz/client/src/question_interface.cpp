#include "../include/question_interface.h"
#include "../include/client.h"
#include <iostream>
#include <string>
#include <limits>

void enterExamRoom(int sock, const std::string& roomId) {
    std::cout << "\n[ROOM " << roomId << "] Joined successfully.\n";
    std::cout << "Waiting for exam to start...\n";

    bool examStarted = false;
    
    while (true) {
        std::string msg = recvLine(sock);
        if (msg.empty()) return;

        auto parts = split(msg, '|');
        if (parts.empty()) continue;

        std::string cmd = parts[0];

        if (cmd == "TEST_STARTED") {
            std::cout << "\n>>> EXAM STARTED! <<<\n";
            if (parts.size() > 1) std::cout << "Time limit: " << parts[1] << " minutes.\n";
            examStarted = true;
            // Continue to process next message (first question)
            continue;
        } else if (cmd == "QUESTION") {
            if (parts.size() < 7) {
                // Invalid question format, skip
                continue;
            }
            std::cout << "\n-----------------------------------\n";
            std::cout << "Question: " << parts[2] << std::endl;
            std::cout << "A. " << parts[3] << std::endl;
            std::cout << "B. " << parts[4] << std::endl;
            std::cout << "C. " << parts[5] << std::endl;
            std::cout << "D. " << parts[6] << std::endl;
            
            std::string ans;
            std::cout << "Your answer (A/B/C/D): ";
            std::cin >> ans;
            
            // Validate answer input
            while (ans != "A" && ans != "a" && ans != "B" && ans != "b" && 
                   ans != "C" && ans != "c" && ans != "D" && ans != "d") {
                std::cout << "Invalid choice! Please enter A, B, C, or D: ";
                std::cin >> ans;
            }
            
            // Convert to uppercase
            if (ans == "a") ans = "A";
            else if (ans == "b") ans = "B";
            else if (ans == "c") ans = "C";
            else if (ans == "d") ans = "D";
            
            sendLine(sock, "ANSWER|" + parts[1] + "|" + ans);
            continue; // Important: skip the else if below

        } else if (cmd == "END_EXAM") {
            std::cout << "\n===================================\n";
            std::cout << "           EXAM FINISHED           \n";
            std::cout << "===================================\n";
            if (parts.size() >= 2) {
                std::cout << "\nYour Score: " << parts[1] << std::endl;
            }
            if (parts.size() >= 3) {
                std::cout << "Correct Answers: " << parts[2] << std::endl;
            }
            std::cout << "\nPress Enter to return to menu...";
            std::cin.ignore();
            std::cin.get();
            break;
        } else if (!examStarted) {
            // Before exam starts, show server messages
            std::cout << "Server: " << msg << std::endl;
        }
    }
}

void studentMenu(int sock) {
    while (true) {
        int choice;
        std::cout << "\n====================================\n";
        std::cout << "           STUDENT MENU\n";
        std::cout << "====================================\n";
        std::cout << "1. View available quizzes\n";
        std::cout << "2. Join Exam (Tham gia thi)\n";
        std::cout << "3. View Exam History\n";
        std::cout << "4. Logout\n";
        std::cout << "====================================\n";
        std::cout << "Enter your choice: ";
        std::cin >> choice;

        if (choice == 1) {
            sendLine(sock, "LIST_QUIZZES");
            std::cout << "Server: " << recvLine(sock) << std::endl;
        } else if (choice == 2) {
            int quizId;
            std::cout << "Enter Quiz ID to join: ";
            std::cin >> quizId;
            sendLine(sock, "JOIN_ROOM|" + std::to_string(quizId));
            std::string resp = recvLine(sock);
            auto parts = split(resp, '|');

            if (parts.size() > 0 && parts[0] == "TEST_STARTED") {
                enterExamRoom(sock, std::to_string(quizId));
            } else if (parts.size() > 0 && parts[0] == "JOIN_FAIL") {
                std::cout << "Failed to join: " << resp << std::endl;
            } else {
                std::cout << "Server: " << resp << std::endl;
            }
        } else if (choice == 3) {
            sendLine(sock, "LIST_MY_HISTORY");
            std::cout << "\n--- YOUR EXAM HISTORY ---\n";
            std::string resp = recvLine(sock);
            if (resp.find("HISTORY|") == 0) {
                std::string data = resp.substr(8);
                auto items = split(data, ';');
                for (auto &i : items) std::cout << "- " << i << std::endl;
            } else {
                std::cout << "Server: " << resp << std::endl;
            }
        } else if (choice == 4) {
            sendLine(sock, "QUIT");
            std::cout << "Logging out...\n";
            break;
        } else {
            std::cout << "Invalid choice.\n";
        }
    }
}

