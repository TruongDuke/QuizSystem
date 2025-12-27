#include "../include/question_manager.h"
#include "../include/db_manager.h"
#include "../include/protocol_manager.h"
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/resultset.h>
#include <sstream>
#include <iostream>

// ==================================================
// QUESTIONS
// ==================================================

// LIST_QUESTIONS|quizId
void handleListQuestions(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 2) {
        sendLine(sock, "LIST_QUESTIONS_FAIL|reason=bad_format");
        return;
    }
    int quizId;
    try {
        quizId = std::stoi(parts[1]);
    } catch (...) {
        sendLine(sock, "LIST_QUESTIONS_FAIL|reason=bad_number");
        return;
    }

    std::string q =
        "SELECT question_id, question_text "
        "FROM Questions WHERE quiz_id=" + std::to_string(quizId) + ";";

    try {
        sql::ResultSet *res = db->executeQuery(q);
        if (!res) {
            sendLine(sock, "LIST_QUESTIONS_FAIL|reason=db_error");
            return;
        }

        std::stringstream ss;
        ss << "QUESTIONS|" << quizId << "|";
        bool first = true;
        while (res->next()) {
            if (!first) ss << ";";
            first = false;
            int qid = res->getInt("question_id");
            std::string text = res->getString("question_text");
            ss << qid << ":" << text;
        }
        delete res;
        sendLine(sock, ss.str());
    } catch (sql::SQLException &e) {
        std::cerr << "[LIST_QUESTIONS] SQL error: " << e.what() << std::endl;
        sendLine(sock, "LIST_QUESTIONS_FAIL|reason=sql_error");
    }
}

// ADD_QUESTION|quizId|content|opt1|opt2|opt3|opt4|correctIndex
void handleAddQuestion(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 8) {
        sendLine(sock, "ADD_QUESTION_FAIL|reason=bad_format");
        return;
    }

    int quizId, correct;
    try {
        quizId  = std::stoi(parts[1]);
        correct = std::stoi(parts[7]); // 1..4
    } catch (...) {
        sendLine(sock, "ADD_QUESTION_FAIL|reason=bad_number");
        return;
    }

    std::string content = escapeSql(parts[2]);
    std::string o1 = escapeSql(parts[3]);
    std::string o2 = escapeSql(parts[4]);
    std::string o3 = escapeSql(parts[5]);
    std::string o4 = escapeSql(parts[6]);
    

    try {
        // 1) Questions
        std::string qInsert =
            "INSERT INTO Questions (quiz_id, question_text, difficulty, topic) "
            "VALUES (" + std::to_string(quizId) + ", '" + content +
            "', 'easy', 'general');";

        int questionId = db->executeInsertAndGetId(qInsert);
        if (questionId == 0) {
            sendLine(sock, "ADD_QUESTION_FAIL|reason=sql_error");
            return;
        }

        // 2) Answers
        auto insertAns = [&](const std::string &txt, int index) {
            std::string a =
                "INSERT INTO Answers (question_id, answer_text, is_correct) "
                "VALUES (" + std::to_string(questionId) + ", '" +
                escapeSql(txt) + "', " + (index == correct ? "1" : "0") + ");";
            db->executeUpdate(a);
        };

        insertAns(o1, 1);
        insertAns(o2, 2);
        insertAns(o3, 3);
        insertAns(o4, 4);

        sendLine(sock, "ADD_QUESTION_OK");
        std::cout << "[ADD_QUESTION] question added for quiz " << quizId
             << ", qid=" << questionId << std::endl;

    } catch (sql::SQLException &e) {
        std::cerr << "[ADD_QUESTION] SQL error: " << e.what() << std::endl;
        sendLine(sock, "ADD_QUESTION_FAIL|reason=sql_error");
    }
}

// EDIT_QUESTION|qId|content|opt1|opt2|opt3|opt4|correctIndex
void handleEditQuestion(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 8) {
        sendLine(sock, "EDIT_QUESTION_FAIL|reason=bad_format");
        return;
    }

    int qid, correct;
    try {
        qid     = std::stoi(parts[1]);
        correct = std::stoi(parts[7]);
    } catch (...) {
        sendLine(sock, "EDIT_QUESTION_FAIL|reason=bad_number");
        return;
    }

    std::string content = escapeSql(parts[2]);
    std::string o1 = escapeSql(parts[3]);
    std::string o2 = escapeSql(parts[4]);
    std::string o3 = escapeSql(parts[5]);
    std::string o4 = escapeSql(parts[6]);

    try {
        // update question text
        std::string q =
            "UPDATE Questions SET question_text='" + content +
            "' WHERE question_id=" + std::to_string(qid) + ";";
        db->executeUpdate(q);

        // xoá toàn bộ answers cũ
        std::string d =
            "DELETE FROM Answers WHERE question_id=" +
            std::to_string(qid) + ";";
        db->executeUpdate(d);

        // insert lại 4 đáp án
        auto insertAns = [&](const std::string &txt, int index) {
            std::string a =
                "INSERT INTO Answers (question_id, answer_text, is_correct) "
                "VALUES (" + std::to_string(qid) + ", '" +
                escapeSql(txt) + "', " + (index == correct ? "1" : "0") + ");";
            db->executeUpdate(a);
        };

        insertAns(o1, 1);
        insertAns(o2, 2);
        insertAns(o3, 3);
        insertAns(o4, 4);

        sendLine(sock, "EDIT_QUESTION_OK");
    } catch (sql::SQLException &e) {
        std::cerr << "[EDIT_QUESTION] SQL error: " << e.what() << std::endl;
        sendLine(sock, "EDIT_QUESTION_FAIL|reason=sql_error");
    }
}

// DELETE_QUESTION|qId
void handleDeleteQuestion(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 2) {
        sendLine(sock, "DELETE_QUESTION_FAIL|reason=bad_format");
        return;
    }
    int qid;
    try {
        qid = std::stoi(parts[1]);
    } catch (...) {
        sendLine(sock, "DELETE_QUESTION_FAIL|reason=bad_number");
        return;
    }

    try {
        std::string dAns =
            "DELETE FROM Answers WHERE question_id=" +
            std::to_string(qid) + ";";
        db->executeUpdate(dAns);

        std::string dQ =
            "DELETE FROM Questions WHERE question_id=" +
            std::to_string(qid) + ";";
        db->executeUpdate(dQ);

        sendLine(sock, "DELETE_QUESTION_OK");
    } catch (sql::SQLException &e) {
        std::cerr << "[DELETE_QUESTION] SQL error: " << e.what() << std::endl;
        sendLine(sock, "DELETE_QUESTION_FAIL|reason=sql_error");
    }
}

// GET_ONE_QUESTION|qId
void handleGetOneQuestion(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 2) {
        sendLine(sock, "GET_ONE_QUESTION_FAIL|reason=bad_format");
        return;
    }
    int qid;
    try {
        qid = std::stoi(parts[1]);
    } catch (...) {
        sendLine(sock, "GET_ONE_QUESTION_FAIL|reason=bad_number");
        return;
    }

    try {
        std::string q = "SELECT question_id, question_text, difficulty, topic FROM Questions WHERE question_id=" + std::to_string(qid) + ";";
        sql::ResultSet *res = db->executeQuery(q);
        
        if (!res || !res->next()) {
            delete res;
            sendLine(sock, "GET_ONE_QUESTION_FAIL|reason=not_found");
            return;
        }
        
        std::string content = res->getString("question_text");
        std::string difficulty = res->getString("difficulty");
        std::string topic = res->getString("topic");
        delete res;
        
        // Get answers
        std::string ans_q = "SELECT answer_text, is_correct FROM Answers WHERE question_id=" + std::to_string(qid) + " ORDER BY answer_id;";
        res = db->executeQuery(ans_q);
        
        std::vector<std::string> options;
        int correct_idx = 1;
        int idx = 1;
        
        while (res && res->next()) {
            options.push_back(res->getString("answer_text"));
            if (res->getBoolean("is_correct")) {
                correct_idx = idx;
            }
            idx++;
        }
        delete res;
        
        // Pad to 4 options
        while (options.size() < 4) {
            options.push_back("");
        }
        
        // ONE_QUESTION|id|content|opt1|opt2|opt3|opt4|correctIdx|difficulty|topic
        std::stringstream ss;
        ss << "ONE_QUESTION|" << qid << "|" << content << "|"
           << options[0] << "|" << options[1] << "|" << options[2] << "|" << options[3] << "|"
           << correct_idx << "|" << difficulty << "|" << topic;
        sendLine(sock, ss.str());
        
    } catch (sql::SQLException &e) {
        std::cerr << "[GET_ONE_QUESTION] SQL error: " << e.what() << std::endl;
        sendLine(sock, "GET_ONE_QUESTION_FAIL|reason=sql_error");
    }
}

// ==================================================
// QUESTION BANK
// ==================================================

// LIST_QUESTION_BANK|topic|difficulty
// Nếu topic hoặc difficulty là "all" hoặc rỗng thì không lọc theo đó
void handleListQuestionBank(const std::vector<std::string> &parts, int sock, DbManager *db) {
    std::string query = "SELECT bank_question_id, question_text, difficulty, topic FROM QuestionBank WHERE 1=1";
    
    // Lọc theo topic nếu có
    if (parts.size() >= 2 && !parts[1].empty() && parts[1] != "all") {
        std::string topic = escapeSql(parts[1]);
        query += " AND topic LIKE '%" + topic + "%'";
    }
    
    // Lọc theo difficulty nếu có
    if (parts.size() >= 3 && !parts[2].empty() && parts[2] != "all") {
        std::string difficulty = escapeSql(parts[2]);
        if (difficulty == "easy" || difficulty == "medium" || difficulty == "hard") {
            query += " AND difficulty = '" + difficulty + "'";
        }
    }
    
    query += " ORDER BY topic, difficulty;";

    try {
        sql::ResultSet *res = db->executeQuery(query);
        if (!res) {
            sendLine(sock, "LIST_QUESTION_BANK_FAIL|reason=db_error");
            return;
        }

        std::stringstream ss;
        ss << "QUESTION_BANK|";
        bool first = true;
        int count = 0;
        while (res->next()) {
            if (!first) ss << ";";
            first = false;
            int bid = res->getInt("bank_question_id");
            std::string text = res->getString("question_text");
            std::string diff = res->getString("difficulty");
            std::string topic = res->getString("topic");
            ss << bid << ":" << text << "[" << diff << "," << topic << "]";
            count++;
        }
        delete res;
        
        if (count == 0) {
            ss << "No questions found";
        }
        
        sendLine(sock, ss.str());
    } catch (sql::SQLException &e) {
        std::cerr << "[LIST_QUESTION_BANK] SQL error: " << e.what() << std::endl;
        sendLine(sock, "LIST_QUESTION_BANK_FAIL|reason=sql_error");
    }
}

// GET_QUESTION_BANK|bankQuestionId
void handleGetQuestionBank(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 2) {
        sendLine(sock, "GET_QUESTION_BANK_FAIL|reason=bad_format");
        return;
    }

    int bankQId;
    try {
        bankQId = std::stoi(parts[1]);
    } catch (...) {
        sendLine(sock, "GET_QUESTION_BANK_FAIL|reason=bad_number");
        return;
    }

    try {
        // Lấy câu hỏi
        std::string qQuery = 
            "SELECT question_text, difficulty, topic FROM QuestionBank "
            "WHERE bank_question_id=" + std::to_string(bankQId) + ";";
        
        sql::ResultSet *qRes = db->executeQuery(qQuery);
        if (!qRes || !qRes->next()) {
            sendLine(sock, "GET_QUESTION_BANK_FAIL|reason=not_found");
            if (qRes) delete qRes;
            return;
        }

        std::string qText = qRes->getString("question_text");
        std::string diff = qRes->getString("difficulty");
        std::string topic = qRes->getString("topic");
        delete qRes;

        // Lấy đáp án
        std::string aQuery = 
            "SELECT answer_text, is_correct FROM BankAnswers "
            "WHERE bank_question_id=" + std::to_string(bankQId) + " "
            "ORDER BY bank_answer_id;";
        
        sql::ResultSet *aRes = db->executeQuery(aQuery);
        if (!aRes) {
            sendLine(sock, "GET_QUESTION_BANK_FAIL|reason=db_error");
            return;
        }

        std::stringstream ss;
        ss << "QUESTION_BANK_DETAIL|" << bankQId << "|" << qText << "|" 
           << diff << "|" << topic << "|";
        
        int correctIndex = 0;
        int index = 1;
        while (aRes->next()) {
            if (index > 1) ss << "|";
            std::string ans = aRes->getString("answer_text");
            bool isCorrect = aRes->getBoolean("is_correct");
            ss << ans;
            if (isCorrect) correctIndex = index;
            index++;
        }
        ss << "|" << correctIndex;
        delete aRes;

        sendLine(sock, ss.str());
    } catch (sql::SQLException &e) {
        std::cerr << "[GET_QUESTION_BANK] SQL error: " << e.what() << std::endl;
        sendLine(sock, "GET_QUESTION_BANK_FAIL|reason=sql_error");
    }
}

// ADD_TO_QUIZ_FROM_BANK|quizId|bankQuestionId
void handleAddToQuizFromBank(const std::vector<std::string> &parts, int sock, DbManager *db) {
    if (parts.size() != 3) {
        sendLine(sock, "ADD_TO_QUIZ_FROM_BANK_FAIL|reason=bad_format");
        return;
    }

    int quizId, bankQId;
    try {
        quizId = std::stoi(parts[1]);
        bankQId = std::stoi(parts[2]);
    } catch (...) {
        sendLine(sock, "ADD_TO_QUIZ_FROM_BANK_FAIL|reason=bad_number");
        return;
    }

    try {
        // Lấy thông tin câu hỏi từ ngân hàng
        std::string qQuery = 
            "SELECT question_text, difficulty, topic FROM QuestionBank "
            "WHERE bank_question_id=" + std::to_string(bankQId) + ";";
        
        sql::ResultSet *qRes = db->executeQuery(qQuery);
        if (!qRes || !qRes->next()) {
            sendLine(sock, "ADD_TO_QUIZ_FROM_BANK_FAIL|reason=question_not_found");
            if (qRes) delete qRes;
            return;
        }

        std::string qText = escapeSql(qRes->getString("question_text"));
        std::string diff = qRes->getString("difficulty");
        std::string topic = escapeSql(qRes->getString("topic"));
        delete qRes;

        // Thêm vào Questions
        std::string insertQ = 
            "INSERT INTO Questions (quiz_id, question_text, difficulty, topic) "
            "VALUES (" + std::to_string(quizId) + ", '" + qText + 
            "', '" + diff + "', '" + topic + "');";
        
        int questionId = db->executeInsertAndGetId(insertQ);
        if (questionId == 0) {
            sendLine(sock, "ADD_TO_QUIZ_FROM_BANK_FAIL|reason=sql_error");
            return;
        }

        // Lấy đáp án từ ngân hàng và thêm vào Answers
        std::string aQuery = 
            "SELECT answer_text, is_correct FROM BankAnswers "
            "WHERE bank_question_id=" + std::to_string(bankQId) + " "
            "ORDER BY bank_answer_id;";
        
        sql::ResultSet *aRes = db->executeQuery(aQuery);
        if (!aRes) {
            std::cerr << "[ADD_TO_QUIZ_FROM_BANK] ERROR: Failed to query answers from bank" << std::endl;
            sendLine(sock, "ADD_TO_QUIZ_FROM_BANK_FAIL|reason=db_error");
            return;
        }

        int answerCount = 0;
        bool hasError = false;
        
        while (aRes->next()) {
            std::string ansText = aRes->getString("answer_text");
            bool isCorrect = aRes->getBoolean("is_correct");
            
            std::cout << "[ADD_TO_QUIZ_FROM_BANK] Copying answer: '" << ansText 
                      << "' (correct=" << (isCorrect ? "yes" : "no") << ")" << std::endl;
            
            std::string escapedText = escapeSql(ansText);
            std::string insertA = 
                "INSERT INTO Answers (question_id, answer_text, is_correct) "
                "VALUES (" + std::to_string(questionId) + ", '" + escapedText + 
                "', " + (isCorrect ? "1" : "0") + ");";
            
            // Thử insert và kiểm tra xem có thành công không
            // Vì executeUpdate không throw, ta cần verify bằng cách query lại
            try {
                db->executeUpdate(insertA);
                
                // Verify: Query lại để đảm bảo answer đã được insert
                std::string verifySql = 
                    "SELECT COUNT(*) as cnt FROM Answers WHERE question_id=" + 
                    std::to_string(questionId) + " AND answer_text='" + escapedText + "';";
                sql::ResultSet* verifyRes = db->executeQuery(verifySql);
                if (verifyRes && verifyRes->next() && verifyRes->getInt("cnt") > 0) {
                    answerCount++;
                    std::cout << "[ADD_TO_QUIZ_FROM_BANK] Answer " << answerCount 
                              << " inserted and verified successfully" << std::endl;
                } else {
                    std::cerr << "[ADD_TO_QUIZ_FROM_BANK] ERROR: Answer insertion failed verification!" << std::endl;
                    hasError = true;
                }
                delete verifyRes;
            } catch (sql::SQLException& e) {
                std::cerr << "[ADD_TO_QUIZ_FROM_BANK] ERROR inserting answer: " 
                          << e.what() << std::endl;
                hasError = true;
            }
        }
        delete aRes;
        
        // Validate: Câu hỏi phải có ít nhất 1 đáp án
        if (answerCount == 0 || hasError) {
            std::cerr << "[ADD_TO_QUIZ_FROM_BANK] ERROR: Failed to copy answers! "
                      << "answerCount=" << answerCount << ", hasError=" << hasError << std::endl;
            // Xóa câu hỏi đã thêm vì không có đáp án hoặc có lỗi
            std::string deleteQ = "DELETE FROM Questions WHERE question_id=" + std::to_string(questionId) + ";";
            db->executeUpdate(deleteQ);
            std::cerr << "[ADD_TO_QUIZ_FROM_BANK] Deleted question " << questionId 
                      << " due to missing answers" << std::endl;
            sendLine(sock, "ADD_TO_QUIZ_FROM_BANK_FAIL|reason=question_has_no_answers");
            return;
        }
        
        std::cout << "[ADD_TO_QUIZ_FROM_BANK] Copied " << answerCount 
                  << " answers from bank to question " << questionId << std::endl;

        sendLine(sock, "ADD_TO_QUIZ_FROM_BANK_OK|questionId=" + std::to_string(questionId));
        std::cout << "[ADD_TO_QUIZ_FROM_BANK] question " << bankQId 
                  << " added to quiz " << quizId << ", new qid=" << questionId << std::endl;

    } catch (sql::SQLException &e) {
        std::cerr << "[ADD_TO_QUIZ_FROM_BANK] SQL error: " << e.what() << std::endl;
        sendLine(sock, "ADD_TO_QUIZ_FROM_BANK_FAIL|reason=sql_error");
    }
}

// ADD_TO_BANK|content|opt1|opt2|opt3|opt4|correctIndex|difficulty|topic
void handleAddToBank(const std::vector<std::string> &parts, int sock, DbManager *db, int userId) {
    if (parts.size() != 9) {
        sendLine(sock, "ADD_TO_BANK_FAIL|reason=bad_format");
        return;
    }

    int correct;
    try {
        correct = std::stoi(parts[6]);
    } catch (...) {
        sendLine(sock, "ADD_TO_BANK_FAIL|reason=bad_number");
        return;
    }

    std::string content = escapeSql(parts[1]);
    std::string o1 = escapeSql(parts[2]);
    std::string o2 = escapeSql(parts[3]);
    std::string o3 = escapeSql(parts[4]);
    std::string o4 = escapeSql(parts[5]);
    std::string diff = escapeSql(parts[7]);
    std::string topic = escapeSql(parts[8]);

    // Validate difficulty
    if (diff != "easy" && diff != "medium" && diff != "hard") {
        sendLine(sock, "ADD_TO_BANK_FAIL|reason=invalid_difficulty");
        return;
    }

    try {
        // Thêm vào QuestionBank
        std::string qInsert = 
            "INSERT INTO QuestionBank (question_text, difficulty, topic, created_by) "
            "VALUES ('" + content + "', '" + diff + "', '" + topic + "', " + 
            std::to_string(userId) + ");";

        int bankQId = db->executeInsertAndGetId(qInsert);
        if (bankQId == 0) {
            sendLine(sock, "ADD_TO_BANK_FAIL|reason=sql_error");
            return;
        }

        // Thêm đáp án
        auto insertAns = [&](const std::string &txt, int index) {
            std::string a = 
                "INSERT INTO BankAnswers (bank_question_id, answer_text, is_correct) "
                "VALUES (" + std::to_string(bankQId) + ", '" + escapeSql(txt) + 
                "', " + (index == correct ? "1" : "0") + ");";
            db->executeUpdate(a);
        };

        insertAns(o1, 1);
        insertAns(o2, 2);
        insertAns(o3, 3);
        insertAns(o4, 4);

        sendLine(sock, "ADD_TO_BANK_OK|bankQuestionId=" + std::to_string(bankQId));
        std::cout << "[ADD_TO_BANK] question added to bank, bid=" << bankQId << std::endl;
    } catch (sql::SQLException &e) {
        std::cerr << "[ADD_TO_BANK] SQL error: " << e.what() << std::endl;
        sendLine(sock, "ADD_TO_BANK_FAIL|reason=sql_error");
    }
}

