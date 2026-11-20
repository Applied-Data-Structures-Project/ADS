// letter_service.cpp
// Same order as the application service:
//   1. check the rules
//   2. change our data structures (HashMaps / Queue)
//   3. ask the repo to save it to the database

#include "services/letter_service.hpp"

using namespace std;

LetterService::LetterService(ApplicationRepo& repo) : repo(repo) {}

bool LetterService::endsWith(const string& text, const string& ending) {
    if (ending.size() > text.size()) return false;
    return text.compare(text.size() - ending.size(), ending.size(), ending) == 0;
}

//  start-up 

void LetterService::loadAll() {
    Vector<LetterRequest> all = repo.loadLetters();

    // 1. Sort by id. The database gives ids 1, 2, 3... as requests arrive, so a smaller id
    //    means an older request. Insertion sort, same as in the application service.
    for (int i = 1; i < all.size(); i++) {
        LetterRequest current = all[i];
        int j = i - 1;
        while (j >= 0 && all[j].id > current.id) {
            all[j + 1] = all[j];
            j--;
        }
        all[j + 1] = current;
    }

    // 2. Fill the HashMaps, and put every Pending request in its doctor's queue (oldest first).
    for (int i = 0; i < all.size(); i++) {
        LetterRequest& l = all[i];
        requests.insert(l.id, l);
        byStudent[l.studentEmail].push_back(l.id);
        if (l.status == "Pending") doctorQueue[l.doctorEmail].push(l.id);
    }
}
// student asks 

int LetterService::requestLetter(const string& studentEmail, const string& doctorName,
                                 const string& doctorEmail, const string& now, string& error) {
    // 1. Rules
    if (doctorName.empty() || doctorEmail.empty()) {
        error = "Enter the doctor's name and email.";
        return -1;
    }
    if (!endsWith(doctorEmail, "@aucegypt.edu")) {
        error = "The letter must come from an AUC faculty member (@aucegypt.edu).";
        return -1;
    }
    if (doctorEmail == studentEmail) {
        error = "You cannot ask yourself for a letter.";
        return -1;
    }
    const Vector<int>* mine = byStudent.get(studentEmail);
    if (mine != nullptr) {
        if (mine->size() >= MAX_REQUESTS) {
            error = "You can send at most " + to_string(MAX_REQUESTS) + " requests.";
            return -1;
        }
        for (int i = 0; i < mine->size(); i++) {
            const LetterRequest* old = requests.get((*mine)[i]);
            if (old != nullptr && old->doctorEmail == doctorEmail) {
                error = "You already asked this doctor.";
                return -1;
            }
        }
    }

    // 2. Save first: the database gives us the new id.
    LetterRequest l;
    l.studentEmail = studentEmail;
    l.doctorName = doctorName;
    l.doctorEmail = doctorEmail;
    l.status = "Pending";
    l.requestedAt = now;
    l.id = repo.insertLetter(l);
    if (l.id == -1) {
        error = "Could not send the request. Please try again.";
        return -1;
    }

    // 3. Our data structures
    requests.insert(l.id, l);
    byStudent[studentEmail].push_back(l.id);
    doctorQueue[doctorEmail].push(l.id);   // joins the back of this doctor's line
    return l.id;
}
//  doctor answers 

bool LetterService::answer(int id, const string& doctorEmail, bool send, const string& letterText,
                           const string& now, string& error) {
    LetterRequest* l = requests.get(id);
    if (l == nullptr) {
        error = "This request does not exist.";
        return false;
    }
    if (l->doctorEmail != doctorEmail) {
        error = "This request was sent to another doctor.";
        return false;
    }
    if (l->status != "Pending") {
        error = "You already answered this request.";
        return false;
    }
    if (send && letterText.empty()) {
        error = "Write the letter before sending it.";
        return false;
    }

    LetterRequest updated = *l;
    updated.status = send ? "Received" : "Declined";
    updated.letterText = send ? letterText : "";
    updated.answeredAt = now;
    if (!repo.updateLetter(updated)) {
        error = "Could not save. Please try again.";
        return false;
    }

    *l = updated;              // update the HashMap
    cleanFront(doctorEmail);   // take answered requests off the front of the queue
    return true;
}

// A doctor may answer any request, not only the oldest. An answered request stays in the
// queue until it reaches the front, then it is removed here.
void LetterService::cleanFront(const string& doctorEmail) {
    Queue<int>* q = doctorQueue.get(doctorEmail);
    if (q == nullptr) return;
    while (!q->empty()) {
        const LetterRequest* l = requests.get(q->front());
        if (l != nullptr && l->status == "Pending") break;
        q->pop();
    }
}
//  reading 

Vector<LetterRequest> LetterService::lettersOf(const string& studentEmail) const {
    Vector<LetterRequest> out;
    const Vector<int>* mine = byStudent.get(studentEmail);
    if (mine == nullptr) return out;
    for (int i = 0; i < mine->size(); i++) {
        const LetterRequest* l = requests.get((*mine)[i]);
        if (l != nullptr) out.push_back(*l);
    }
    return out;
}

Vector<LetterRequest> LetterService::pendingFor(const string& doctorEmail) {
    Vector<LetterRequest> out;
    cleanFront(doctorEmail);
    Queue<int>* q = doctorQueue.get(doctorEmail);
    if (q == nullptr) return out;

    // Walk the queue without losing it: take each id from the front and put it back at the end.
    int n = q->size();
    for (int i = 0; i < n; i++) {
        int id = q->front();
        q->pop();
        const LetterRequest* l = requests.get(id);
        if (l != nullptr && l->status == "Pending") out.push_back(*l);
        q->push(id);
    }
    return out;
}

bool LetterService::hasReceivedLetter(const string& studentEmail) const {
    const Vector<int>* mine = byStudent.get(studentEmail);
    if (mine == nullptr) return false;
    for (int i = 0; i < mine->size(); i++) {
        const LetterRequest* l = requests.get((*mine)[i]);
        if (l != nullptr && l->status == "Received") return true;
    }
    return false;
}