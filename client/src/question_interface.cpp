#include "../include/question_interface.h"
#include "../include/client.h"
#include <iostream>
#include <string>
#include <limits>
#include <cctype>
#include <sys/select.h>
#include <unistd.h>
#include <vector>

// Helper function to display current question
void displayQuestion(int currentIndex, int totalQuestions, int currentQuestionId,
                     const std::string& questionText, 
                     const std::string& optA, const std::string& optB,
                     const std::string& optC, const std::string& optD,
                     const std::string& previousAnswer,
                     const std::vector<bool>& answeredQuestions) {
    std::cout << "\n===================================\n";
    std::cout << "  Question " << (currentIndex + 1) << " / " << totalQuestions << "\n";
    std::cout << "===================================\n";
    std::cout << questionText << std::endl;
    std::cout << "-----------------------------------\n";
    std::cout << "A. " << optA << std::endl;
    std::cout << "B. " << optB << std::endl;
    std::cout << "C. " << optC << std::endl;
    std::cout << "D. " << optD << std::endl;
    
    if (!previousAnswer.empty()) {
        std::string prevChoice = "";
        if (previousAnswer == optA) prevChoice = "A";
        else if (previousAnswer == optB) prevChoice = "B";
        else if (previousAnswer == optC) prevChoice = "C";
        else if (previousAnswer == optD) prevChoice = "D";
        if (!prevChoice.empty()) {
            std::cout << "[Your current answer: " << prevChoice << "]\n";
        }
    }
    
    int answeredCount = 0;
    for (bool a : answeredQuestions) if (a) answeredCount++;
    std::cout << "-----------------------------------\n";
    std::cout << "Progress: " << answeredCount << "/" << totalQuestions << " answered\n";
    std::cout << "[N]ext [P]rev [G#]Go to [S]ubmit\n";
    std::cout << "Your choice (A/B/C/D/N/P/G#/S): ";
    std::cout.flush();
}
 
void enterExamRoom(int sock, const std::string& roomId, int totalQuestions) {
    std::cout << "\n[ROOM " << roomId << "] Joined successfully.\n";
    std::cout << "Total questions: " << totalQuestions << "\n";
    std::cout << "\n=== NAVIGATION INSTRUCTIONS ===\n";
    std::cout << "  A/B/C/D  - Answer the question\n";
    std::cout << "  N        - Next question\n";
    std::cout << "  P        - Previous question\n";
    std::cout << "  G<num>   - Go to question (e.g., G5 for question 5)\n";
    std::cout << "  S        - Submit exam\n";
    std::cout << "================================\n";
 
    int currentIndex = 0;
    int currentQuestionId = 0;
    std::string currentQuestionText, optA, optB, optC, optD, currentPrevAnswer;
    std::vector<bool> answeredQuestions(totalQuestions, false);
    bool waitingForQuestion = true; // Start by waiting for first question
    
    while (true) {
        fd_set readfds;
        struct timeval timeout;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        if (!waitingForQuestion) {
            FD_SET(0, &readfds); // Only check stdin when we have a question displayed
        }
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000; // 100ms
        
        int maxfd = (sock > 0) ? sock : 0;
        int result = select(maxfd + 1, &readfds, NULL, NULL, &timeout);
        
        if (result > 0) {
            // Check socket for server messages
            if (FD_ISSET(sock, &readfds)) {
                std::string msg = recvLine(sock);
                if (msg.empty()) {
                    std::cout << "\n[Connection closed]\n";
                    return;
                }
                
                auto parts = split(msg, '|');
                if (parts.empty()) continue;
                
                std::string cmd = parts[0];
                
                if (cmd == "QUESTION") {
                    // Format: QUESTION|qId|text|A|B|C|D|currentIndex|totalQuestions|previousAnswer
                    if (parts.size() < 7) continue;
                    
                    currentQuestionId = std::stoi(parts[1]);
                    currentQuestionText = parts[2];
                    optA = parts[3];
                    optB = parts[4];
                    optC = parts[5];
                    optD = parts[6];
                    
                    if (parts.size() >= 9) {
                        currentIndex = std::stoi(parts[7]);
                    }
                    
                    currentPrevAnswer = "";
                    if (parts.size() >= 10 && !parts[9].empty()) {
                        currentPrevAnswer = parts[9];
                        answeredQuestions[currentIndex] = true;
                    }
                    
                    displayQuestion(currentIndex, totalQuestions, currentQuestionId,
                                    currentQuestionText, optA, optB, optC, optD,
                                    currentPrevAnswer, answeredQuestions);
                    waitingForQuestion = false;
                    
                } else if (cmd == "SAVE_ANSWER_OK") {
                    std::cout << "[Answer saved]\n";
                    std::cout << "Your choice (A/B/C/D/N/P/G#/S): ";
                    std::cout.flush();
                    
                } else if (cmd == "SAVE_ANSWER_FAIL" || cmd == "GO_TO_QUESTION_FAIL") {
                    std::string reason = parts.size() > 1 ? parts[1] : "unknown_error";
                    std::cout << "\n[ERROR] " << reason << "\n";
                    std::cout << "Your choice (A/B/C/D/N/P/G#/S): ";
                    std::cout.flush();
                    
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
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    return;
                    
                } else {
                    std::cout << "\n[Server] " << msg << std::endl;
                }
            }
            
            // Check stdin for user input (only when not waiting for question)
            if (!waitingForQuestion && FD_ISSET(0, &readfds)) {
                std::string input;
                std::cin >> input;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                
                if (input.empty()) continue;
                
                char firstChar = toupper(input[0]);
                std::cout << "[DEBUG] Input received: '" << input << "', firstChar='" << firstChar << "'\n";
                
                if (firstChar == 'A' || firstChar == 'B' || 
                    firstChar == 'C' || firstChar == 'D') {
                    // Save answer (don't wait for response here, handle it in socket check)
                    std::string saveMsg = "SAVE_ANSWER|" + std::to_string(currentQuestionId) + "|" + firstChar;
                    std::cout << "[DEBUG] Sending: " << saveMsg << "\n";
                    sendLine(sock, saveMsg);
                    answeredQuestions[currentIndex] = true;
                    // Update local previous answer display
                    if (firstChar == 'A') currentPrevAnswer = optA;
                    else if (firstChar == 'B') currentPrevAnswer = optB;
                    else if (firstChar == 'C') currentPrevAnswer = optC;
                    else if (firstChar == 'D') currentPrevAnswer = optD;
                    
                } else if (firstChar == 'N') {
                    if (currentIndex < totalQuestions - 1) {
                        std::string goMsg = "GO_TO_QUESTION|" + std::to_string(currentIndex + 1);
                        std::cout << "[DEBUG] Sending: " << goMsg << "\n";
                        sendLine(sock, goMsg);
                        waitingForQuestion = true;
                    } else {
                        std::cout << "[Already at last question]\n";
                        std::cout << "Your choice (A/B/C/D/N/P/G#/S): ";
                        std::cout.flush();
                    }
                    
                } else if (firstChar == 'P') {
                    if (currentIndex > 0) {
                        sendLine(sock, "GO_TO_QUESTION|" + std::to_string(currentIndex - 1));
                        waitingForQuestion = true;
                    } else {
                        std::cout << "[Already at first question]\n";
                        std::cout << "Your choice (A/B/C/D/N/P/G#/S): ";
                        std::cout.flush();
                    }
                    
                } else if (firstChar == 'G') {
                    if (input.length() > 1) {
                        try {
                            int targetQ = std::stoi(input.substr(1));
                            if (targetQ >= 1 && targetQ <= totalQuestions) {
                                sendLine(sock, "GO_TO_QUESTION|" + std::to_string(targetQ - 1));
                                waitingForQuestion = true;
                            } else {
                                std::cout << "[Invalid question number. Enter 1-" << totalQuestions << "]\n";
                                std::cout << "Your choice (A/B/C/D/N/P/G#/S): ";
                                std::cout.flush();
                            }
                        } catch (...) {
                            std::cout << "[Invalid input. Use G followed by number (e.g., G5)]\n";
                            std::cout << "Your choice (A/B/C/D/N/P/G#/S): ";
                            std::cout.flush();
                        }
                    } else {
                        std::cout << "[Enter question number after G (e.g., G5)]\n";
                        std::cout << "Your choice (A/B/C/D/N/P/G#/S): ";
                        std::cout.flush();
                    }
                    
                } else if (firstChar == 'S') {
                    int answeredCount = 0;
                    for (bool a : answeredQuestions) if (a) answeredCount++;
                    
                    std::cout << "\n=== CONFIRM SUBMISSION ===\n";
                    std::cout << "Answered: " << answeredCount << "/" << totalQuestions << "\n";
                    if (answeredCount < totalQuestions) {
                        std::cout << "WARNING: You have " << (totalQuestions - answeredCount) 
                                  << " unanswered questions!\n";
                    }
                    std::cout << "Are you sure you want to submit? (Y/N): ";
                    std::cout.flush();
                    
                    char confirm;
                    std::cin >> confirm;
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    
                    if (toupper(confirm) == 'Y') {
                        sendLine(sock, "SUBMIT_EXAM_NOW");
                        waitingForQuestion = true; // Wait for END_EXAM
                    } else {
                        std::cout << "[Submission cancelled]\n";
                        std::cout << "Your choice (A/B/C/D/N/P/G#/S): ";
                        std::cout.flush();
                    }
                    
                } else {
                    std::cout << "[Invalid choice]\n";
                    std::cout << "Your choice (A/B/C/D/N/P/G#/S): ";
                    std::cout.flush();
                }
            }
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
                // Display exam info before entering exam room
                std::cout << "\n>>> EXAM STARTED! <<<\n";
                int timeLimitSeconds = 0;
                int totalQuestions = 0;
                
                if (parts.size() > 1) {
                    timeLimitSeconds = std::stoi(parts[1]);
                    int minutes = timeLimitSeconds / 60;
                    int seconds = timeLimitSeconds % 60;
                    std::cout << "Time limit: " << minutes << " minutes";
                    if (seconds > 0) std::cout << " " << seconds << " seconds";
                    std::cout << std::endl;
                }
                if (parts.size() > 2) {
                    std::string examType = parts[2];
                    if (examType == "scheduled") {
                        std::cout << "[Scheduled Exam] Thời gian làm bài có thể bị giới hạn bởi thời gian kết thúc bài thi.\n";
                    }
                }
                if (parts.size() > 3) {
                    totalQuestions = std::stoi(parts[3]);
                }
                
                enterExamRoom(sock, std::to_string(quizId), totalQuestions);
            } else if (parts.size() > 0 && parts[0] == "JOIN_FAIL") {
                std::cout << "Failed to join: " << resp << std::endl;
                // Show more details for scheduled exam errors
                for (const auto& p : parts) {
                    if (p.find("start_time=") == 0) {
                        std::cout << "  - Exam starts at: " << p.substr(11) << std::endl;
                    } else if (p.find("end_time=") == 0) {
                        std::cout << "  - Exam ends at: " << p.substr(9) << std::endl;
                    } else if (p.find("current_time=") == 0) {
                        std::cout << "  - Current time: " << p.substr(13) << std::endl;
                    }
                }
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