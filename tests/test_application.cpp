// test_application.cpp
// Tests for the application service. Run with: ctest --test-dir build --output-on-failure
// The test passes when every CHECK passes (main returns 0).

#include "test_helpers.hpp"
#include "services/application_service.hpp"

using namespace std;

// A complete application for Salma: everything filled except the 4 documents and the letter.
static Application salmaApplication() {
    Application a;
    a.studentEmail = "salma@aucegypt.edu";
    a.firstName = "Salma";
    a.surname = "Ali";
    a.dateOfBirth = "2004-03-01";
    a.nationality = "Egyptian";
    a.gender = "Female";
    a.phone = "01000000000";
    a.programType = "Exchange";
    a.guardianName = "Mona Ali";
    a.guardianPhone = "01100000000";
    a.guardianEmail = "mona@example.com";
    a.pref[0] = "university-of-toronto";
    a.pref[1] = "university-of-melbourne";
    a.pref[2] = "kyoto-university";
    for (int i = 0; i < 350; i++) a.qGoals += "word ";
    for (int i = 0; i < 400; i++) a.qAdaptability += "word ";
    a.qPreparedness = "Yes";
    a.qTeamwork = "Yes";
    a.qReflection = "Yes";
    a.qFinance = "Yes";
    a.travelExperience = "No";
    a.declaration = true;
    return a;
}

static void uploadAll(ApplicationService& s, const string& email) {
    string err;
    s.uploadDocument(email, "courses", "uploads/courses.pdf", "2026-11-20T10:00:00", err);
    s.uploadDocument(email, "transcript", "uploads/transcript.pdf", "2026-11-20T10:00:00", err);
    s.uploadDocument(email, "cv", "uploads/cv.pdf", "2026-11-20T10:00:00", err);
    s.uploadDocument(email, "passport", "uploads/passport.pdf", "2026-11-20T10:00:00", err);
}

int main() {
    sqlite3* db = openTestDb();
    ApplicationRepo repo(db);
    ApplicationService service(repo);
    service.loadAll();
    string err;

    printf("Eligibility\n");
    StudentInfo s;
    s.level = "UG"; s.declared = true; s.credits = 30; s.gpa = 3.0;
    CHECK(service.checkEligibility(s) == "", "UG with GPA 3.0 and 30 credits can apply");
    s.gpa = 2.5;
    CHECK(service.checkEligibility(s) != "", "UG with GPA 2.5 is refused");
    s.level = "GR"; s.gpa = 3.0;
    CHECK(service.checkEligibility(s) != "", "GR with GPA 3.0 is refused (needs 3.2)");
    s.level = "UG"; s.gpa = 3.0; s.credits = 20;
    CHECK(service.checkEligibility(s) != "", "fewer than 24 credits is refused");
    s.credits = 30; s.declared = false;
    CHECK(service.checkEligibility(s) != "", "undeclared major is refused");

    printf("Preferences\n");
    Application a = salmaApplication();
    CHECK(service.checkPreferences(a) == "", "3 different universities are fine");
    a.pref[1] = a.pref[0];
    CHECK(service.checkPreferences(a) != "", "the same university twice is refused");
    a = salmaApplication();
    a.pref[2] = "";
    CHECK(service.checkPreferences(a) != "", "an empty choice is refused");

    printf("Word count\n");
    CHECK(ApplicationService::wordCount("") == 0, "empty text has 0 words");
    CHECK(ApplicationService::wordCount("I love Toronto") == 3, "3 words");
    CHECK(ApplicationService::wordCount("  I   love\nToronto  ") == 3, "extra spaces and new lines");

    printf("Drafts and documents\n");
    a = salmaApplication();
    CHECK(service.saveDraft(a, "2026-11-20T09:00:00", err), "Salma saves a draft");
    CHECK(service.find("salma@aucegypt.edu") != nullptr, "her application is in the HashMap");
    CHECK(service.find("nobody@aucegypt.edu") == nullptr, "a student who never started is not found");
    a.phone = "01222222222";
    service.saveDraft(a, "2026-11-20T09:30:00", err);
    Application* found = service.find("salma@aucegypt.edu");
    CHECK(found != nullptr && found->phone == "01222222222", "saving again updates her phone");
    CHECK(found != nullptr && found->createdAt == "2026-11-20T09:00:00", "createdAt keeps the first save");

    CHECK(service.missing("salma@aucegypt.edu", false).size() == 5, "4 documents + letter are missing");
    uploadAll(service, "salma@aucegypt.edu");
    CHECK(service.hasDocument("salma@aucegypt.edu", "cv"), "the cv is uploaded");
    service.removeDocument("salma@aucegypt.edu", "cv", err);
    CHECK(!service.hasDocument("salma@aucegypt.edu", "cv"), "the cv is removed");
    uploadAll(service, "salma@aucegypt.edu");
    CHECK(service.missing("salma@aucegypt.edu", false).size() == 1, "only the letter is missing");
    CHECK(service.missing("salma@aucegypt.edu", true).size() == 0, "with a letter, nothing is missing");

    printf("Submit\n");
    CHECK(!service.submit("salma@aucegypt.edu", false, "2026-11-20T10:00:00", err), "no letter: submit refused");
    CHECK(!service.submit("salma@aucegypt.edu", true, "2026-12-01T09:00:00", err), "after the deadline: refused");
    CHECK(service.submit("salma@aucegypt.edu", true, "2026-11-20T11:00:00", err), "Salma submits");
    found = service.find("salma@aucegypt.edu");
    CHECK(found != nullptr && found->status == "Submitted", "her status is Submitted");
    CHECK(!service.saveDraft(a, "2026-11-20T12:00:00", err), "she cannot edit after submitting");
    CHECK(!service.uploadDocument("salma@aucegypt.edu", "cv", "x.pdf", "2026-11-20T12:00:00", err),
          "she cannot change files after submitting");
    CHECK(!service.submit("salma@aucegypt.edu", true, "2026-11-20T12:00:00", err), "she cannot submit twice");

    printf("Review queue (FIFO)\n");
    Application omar = salmaApplication();
    omar.studentEmail = "omar@aucegypt.edu";
    omar.firstName = "Omar";
    service.saveDraft(omar, "2026-11-20T09:00:00", err);
    uploadAll(service, "omar@aucegypt.edu");
    service.submit("omar@aucegypt.edu", true, "2026-11-20T11:30:00", err);
    CHECK(service.waitingForReview() == 2, "2 students are waiting");
    CHECK(service.nextForReview() == "salma@aucegypt.edu", "Salma submitted first, so she is first");
    service.finishReview();
    CHECK(service.nextForReview() == "omar@aucegypt.edu", "then Omar");

    printf("Restart (loadAll)\n");
    ApplicationService restarted(repo);
    restarted.loadAll();
    found = restarted.find("salma@aucegypt.edu");
    CHECK(found != nullptr, "Salma is loaded back");
    CHECK(found != nullptr && found->status == "Submitted", "still Submitted");
    CHECK(restarted.hasDocument("salma@aucegypt.edu", "passport"), "her documents are loaded back");
    CHECK(restarted.waitingForReview() == 2, "the queue is rebuilt");
    CHECK(restarted.nextForReview() == "salma@aucegypt.edu", "in submit order");

    sqlite3_close(db);
    printf("%s (%d failed)\n", failures == 0 ? "ALL PASSED" : "SOME FAILED", failures);
    return failures == 0 ? 0 : 1;
}
