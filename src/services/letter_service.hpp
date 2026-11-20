#pragma once

// letter_service.hpp
// Recommendation letters: a student asks an AUC doctor, the doctor sends or declines.
//
// Data structures:
//   HashMap<int, LetterRequest>       request id -> the request        (find any request in O(1))
//   HashMap<string, Vector<int>>      student email -> their request ids (at most 2)
//   HashMap<string, Queue<int>>       doctor email -> pending request ids, oldest first

#include <string>
#include "hashmap.hpp"
#include "queue.hpp"
#include "vector.hpp"
#include "models/application.hpp"
#include "repo/application_repo.hpp"
class LetterService {
public:
    explicit LetterService(ApplicationRepo& repo);

    // At start-up: fill the HashMaps and every doctor's Queue from the database.
    void loadAll();

    // A student asks a doctor. Returns the new request id, or -1 with the reason in error.
    int requestLetter(const std::string& studentEmail, const std::string& doctorName,
                      const std::string& doctorEmail, const std::string& now, std::string& error);

    // The doctor answers: send = true (with the letter) or false (declined).
    bool answer(int id, const std::string& doctorEmail, bool send, const std::string& letterText,
                const std::string& now, std::string& error);

    // For the student's letters page.
    Vector<LetterRequest> lettersOf(const std::string& studentEmail) const;

    // For the doctor's page: pending requests, oldest first.
    Vector<LetterRequest> pendingFor(const std::string& doctorEmail);

    // Used by submit: has at least one doctor sent a letter?
    bool hasReceivedLetter(const std::string& studentEmail) const;

private:
    ApplicationRepo& repo;
    HashMap<int, LetterRequest> requests;
    HashMap<std::string, Vector<int>> byStudent;
    HashMap<std::string, Queue<int>> doctorQueue;

    static const int MAX_REQUESTS = 2;

    void cleanFront(const std::string& doctorEmail);
    static bool endsWith(const std::string& text, const std::string& ending);
};