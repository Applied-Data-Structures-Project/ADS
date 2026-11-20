// test_emails.cpp
// Group 3 - Malak
//
// Checks that the letter service sends the right emails at the right time.
// Uses FakeNotifier, so no real email is sent.

#include "test_helpers.hpp"
#include "services/letter_service.hpp"

using namespace std;

static bool contains(const string& text, const string& part) {
    return text.find(part) != string::npos;
}

int main() {
    sqlite3* db = openTestDb();
    ApplicationRepo repo(db);
    LetterService letters(repo);
    letters.loadAll();
    FakeNotifier fake;
    letters.setNotifier(&fake, "http://localhost:5173", "30 November");
    string err;
    const string now = "2026-11-20T09:00:00";

    printf("Email to the doctor\n");
    int id = letters.requestLetter("salma@aucegypt.edu", "Dr. Hany", "hany@aucegypt.edu", now, err,
                                   "Salma Ali");
    CHECK(fake.sent.size() == 1, "one email is sent when Salma asks");
    if (fake.sent.size() >= 1) {
        CHECK(fake.sent[0].to == "hany@aucegypt.edu", "it goes to Dr. Hany");
        CHECK(contains(fake.sent[0].subject, "Salma Ali"), "the subject names the student");
        CHECK(contains(fake.sent[0].body, "http://localhost:5173/doctor"), "it has the link to the doctor page");
        CHECK(contains(fake.sent[0].body, "30 November"), "it says the deadline");
    }

    letters.requestLetter("salma@aucegypt.edu", "Dr. Hany", "hany@aucegypt.edu", now, err, "Salma Ali");
    CHECK(fake.sent.size() == 1, "no email when the request is refused");

    printf("Email to the student\n");
    letters.answer(id, "hany@aucegypt.edu", true, "Salma is hard-working.", now, err);
    CHECK(fake.sent.size() == 2, "one email is sent when Dr. Hany answers");
    if (fake.sent.size() >= 2) {
        CHECK(fake.sent[1].to == "salma@aucegypt.edu", "it goes to Salma");
        CHECK(contains(fake.sent[1].subject, "has sent"), "it says the letter was sent");
        CHECK(!contains(fake.sent[1].body, "Salma is hard-working."), "the letter itself stays private");
    }

    int id2 = letters.requestLetter("omar@aucegypt.edu", "Dr. Rana", "rana@aucegypt.edu", now, err);
    letters.answer(id2, "rana@aucegypt.edu", false, "", now, err);
    CHECK(fake.sent.size() == 4, "Omar's request and Dr. Rana's decline send 2 more emails");
    if (fake.sent.size() >= 4) {
        CHECK(contains(fake.sent[2].subject, "omar@aucegypt.edu"), "without a name, the email is used");
        CHECK(contains(fake.sent[3].subject, "cannot write"), "a decline tells the student");
        CHECK(contains(fake.sent[3].body, "/letters"), "and links to the Letters page");
    }

    printf("Without a notifier\n");
    LetterService quiet(repo);
    quiet.loadAll();
    CHECK(quiet.requestLetter("nour@aucegypt.edu", "Dr. Hany", "hany@aucegypt.edu", now, err) > 0,
          "requests still work with no emails set up");

    sqlite3_close(db);
    printf("%s (%d failed)\n", failures == 0 ? "ALL PASSED" : "SOME FAILED", failures);
    return failures == 0 ? 0 : 1;
}
