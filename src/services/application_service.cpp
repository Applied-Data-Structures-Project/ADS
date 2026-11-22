// application_service.cpp
// Every change follows the same order:
//   1. check the rules
//   2. change our data structure (HashMap / Queue)
//   3. ask the repo to save it to the database

#include "services/application_service.hpp"
using namespace std;

static const char* DOC_TYPES[4] = {"courses", "transcript", "cv", "passport"};


ApplicationService::ApplicationService(ApplicationRepo& repo, ApplicationRules rules)
    : repo(repo), rules(rules) {}

string ApplicationService::docKey(const string& email, const string& docType) {
    return email + "|" + docType;
}

// Fills the HashMaps and the Queue from the database. Called once when the server starts.
void ApplicationService::loadAll() {
    // 1. Every application goes into the HashMap: email -> application.
    Vector<Application> all = repo.loadApplications();
    Vector<Application> submitted;
    for (int i = 0; i < all.size(); i++) {
        applications.insert(all[i].studentEmail, all[i]);
        if (all[i].status != "Draft") submitted.push_back(all[i]);
    }

    // 2. Submitted ones go back into the queue in the order they were submitted.
    //    Insertion sort by submittedAt (the dates are saved as text that sorts correctly).
    for (int i = 1; i < submitted.size(); i++) {
        Application current = submitted[i];
        int j = i - 1;
        while (j >= 0 && submitted[j].submittedAt > current.submittedAt) {
            submitted[j + 1] = submitted[j];
            j--;
        }
        submitted[j + 1] = current;
    }
    for (int i = 0; i < submitted.size(); i++) {
        if (submitted[i].status == "Submitted") reviewQueue.push(submitted[i].studentEmail);
    }

    // 3. Every uploaded file goes into the documents HashMap.
    Vector<ApplicationDocument> docs = repo.loadDocuments();
    for (int i = 0; i < docs.size(); i++) {
        documents.insert(docKey(docs[i].studentEmail, docs[i].docType), docs[i].filePath);
    }
}
Application* ApplicationService::find(const string& email) {
    return applications.get(email);   // O(1) on average
}

// eligibility 

string ApplicationService::checkEligibility(const StudentInfo& s) const {
    if (s.level != "GR") {
        if (!s.declared) return "Only students with an officially declared major can apply.";
        if (s.credits < rules.minCredits)
            return "You need at least " + to_string(rules.minCredits) + " credits.";
    }
    double minGpa = (s.level == "GR") ? rules.minGpaGR : rules.minGpaUG;
    if (s.gpa < minGpa) return "Your GPA is below the minimum for " + s.level + " students.";
    return "";
}

// drafts

bool ApplicationService::saveDraft(const Application& a, const string& now, string& error) {
    Application* existing = applications.get(a.studentEmail);

    // Rule: once submitted, nothing can change.
    if (existing != nullptr && existing->status != "Draft") {
        error = "Your application is submitted and can no longer be edited.";
        return false;
    }

    Application updated = a;
    updated.status = "Draft";
    updated.prior = (a.travelExperience == "Yes, Study Abroad");
    updated.createdAt = (existing != nullptr) ? existing->createdAt : now;
    updated.updatedAt = now;

    applications.insert(updated.studentEmail, updated);   // update the HashMap
    if (!repo.saveApplication(updated)) {                  // then save to the database
        error = "Could not save. Please try again.";
        return false;
    }
    return true;
}
// preferences 

string ApplicationService::checkPreferences(const Application& a) const {
    for (int i = 0; i < 3; i++) {
        if (a.pref[i].empty()) return "Choose your #" + to_string(i + 1) + " university.";
        for (int j = 0; j < i; j++) {
            if (a.pref[i] == a.pref[j])
                return "Your #" + to_string(i + 1) + " choice is the same as your #" +
                       to_string(j + 1) + ".";
        }
    }
    return "";
}

// essays

int ApplicationService::wordCount(const string& text) {
    int count = 0;
    bool inWord = false;
    for (char c : text) {
        bool space = (c == ' ' || c == '\n' || c == '\t' || c == '\r');
        if (!space && !inWord) count++;   // a new word starts here
        inWord = !space;
    }
    return count;
}
// documents 

bool ApplicationService::uploadDocument(const string& email, const string& docType,
                                        const string& filePath, const string& now,
                                        string& error) {
    Application* a = applications.get(email);
    if (a != nullptr && a->status != "Draft") {
        error = "Your application is submitted, so files can no longer change.";
        return false;
    }

    documents.insert(docKey(email, docType), filePath);   // add or replace in the HashMap

    ApplicationDocument d;
    d.studentEmail = email;
    d.docType = docType;
    d.filePath = filePath;
    d.uploadedAt = now;
    if (!repo.saveDocument(d)) {
        error = "Could not save the file. Please try again.";
        return false;
    }
    return true;
}

bool ApplicationService::removeDocument(const string& email, const string& docType,
                                        string& error) {
    Application* a = applications.get(email);
    if (a != nullptr && a->status != "Draft") {
        error = "Your application is submitted, so files can no longer change.";
        return false;
    }
    documents.remove(docKey(email, docType));
    return repo.deleteDocument(email, docType);
}

bool ApplicationService::hasDocument(const string& email, const string& docType) const {
    return documents.search(docKey(email, docType));
} 
// submit 

Vector<string> ApplicationService::missing(const string& email, bool letterReceived) const {
    Vector<string> out;
    const Application* a = applications.get(email);
    if (a == nullptr) {
        out.push_back("Start your application");
        return out;
    }

    // Personal info and guardian
    if (a->firstName.empty() || a->surname.empty() || a->dateOfBirth.empty() ||
        a->nationality.empty() || a->gender.empty() || a->phone.empty())
        out.push_back("Complete your personal information");
    if (a->programType.empty()) out.push_back("Choose Exchange or Direct Enrollment");
    if (a->guardianName.empty() || a->guardianPhone.empty() || a->guardianEmail.empty())
        out.push_back("Add your parent or guardian");

    // Preferences
    string prefProblem = checkPreferences(*a);
    if (!prefProblem.empty()) out.push_back(prefProblem);

    // Essays
    int goals = wordCount(a->qGoals);
    if (goals < rules.goalsMinWords || goals > rules.goalsMaxWords)
        out.push_back("Academic goals essay must be " + to_string(rules.goalsMinWords) + "-" +
                      to_string(rules.goalsMaxWords) + " words (now " + to_string(goals) + ")");
    int adapt = wordCount(a->qAdaptability);
    if (adapt < rules.adaptMinWords || adapt > rules.adaptMaxWords)
        out.push_back("Adaptability essay needs at least " + to_string(rules.adaptMinWords) +
                      " words (now " + to_string(adapt) + ")");
    if (a->qPreparedness.empty() || a->qTeamwork.empty() || a->qReflection.empty() ||
        a->qFinance.empty() || a->travelExperience.empty())
        out.push_back("Answer every question in the questionnaire");

    // Documents and letter
    for (int i = 0; i < 4; i++) {
        if (!hasDocument(email, DOC_TYPES[i])) out.push_back(string("Upload your ") + DOC_TYPES[i]);
    }
    if (!letterReceived) out.push_back("Receive a recommendation letter");

    if (!a->declaration) out.push_back("Tick the declaration");
    return out;
}

bool ApplicationService::submit(const string& email, bool letterReceived, const string& now,
                                string& error) {
    Application* a = applications.get(email);
    if (a == nullptr) {
        error = "Start your application first.";
        return false;
    }
    if (a->status != "Draft") {
        error = "You have already submitted.";
        return false;
    }
    if (now > rules.deadline) {
        error = "The deadline has passed. Late applications are not considered.";
        return false;
    }
    Vector<string> todo = missing(email, letterReceived);
    if (todo.size() > 0) {
        error = "Still missing: " + todo[0];
        return false;
    }

    a->status = "Submitted";            // 1. lock it in the HashMap
    a->submittedAt = now;
    a->updatedAt = now;
    reviewQueue.push(email);            // 2. join the review queue (FIFO)
    if (!repo.saveApplication(*a)) {    // 3. save to the database
        error = "Could not save. Please try again.";
        return false;
    }
    return true;
}

// review queue 

int ApplicationService::waitingForReview() const {
    return reviewQueue.size();
}

string ApplicationService::nextForReview() const {
    if (reviewQueue.empty()) return "";
    return reviewQueue.front();
}

void ApplicationService::finishReview() {
    reviewQueue.pop();
}