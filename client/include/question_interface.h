#ifndef QUESTION_INTERFACE_H
#define QUESTION_INTERFACE_H

#include <string>

// Student menu
void studentMenu(int sock);

// Enter exam room with navigation support
void enterExamRoom(int sock, const std::string& roomId, int totalQuestions);

#endif // QUESTION_INTERFACE_H

