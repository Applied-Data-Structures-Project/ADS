// test_letters.cpp

// Tests for the letter service. Run with: ctest --test-dir build --output-on-failure

#include "test_helpers.hpp"
#include "services/letter_service.hpp"

using namespace std;

int main() {
    sqlite3* db = openTestDb();
    ApplicationRepo repo(db);
    LetterService letters(repo);
    letters.loadAll();
    string err;
    const string salma = "salma@aucegypt.edu", omar = "omar@aucegypt.edu";
    const string hany = "hany@aucegypt.edu", rana = "rana@aucegypt.edu";
    const string now = "2026-11-20T09:00:00";

    printf("Asking for a letter\n");
    int first = letters.requestLetter(salma, "Dr. Hany", hany, now, err);
    CHECK(first > 0, "Salma asks Dr. Hany and gets an id");
    int second = letters.requestLetter(omar, "Dr. Hany", hany, now, err);
    CHECK(second > first, "Omar asks Dr. Hany and gets a bigger id");
    CHECK(letters.requestLetter(salma, "Dr. Hany", hany, now, err) == -1, "the same doctor twice is refused");
    CHECK(letters.requestLetter(salma, "X", "x@gmail.com", now, err) == -1, "a non-AUC email is refused");
    CHECK(letters.requestLetter(salma, "Me", salma, now, err) == -1, "asking yourself is refused");
    CHECK(letters.requestLetter(salma, "", "", now, err) == -1, "an empty name or email is refused");
    int third = letters.requestLetter(salma, "Dr. Rana", rana, now, err);
    CHECK(third > 0, "Salma asks Dr. Rana");
    CHECK(letters.requestLetter(salma, "Dr. Ali", "ali@aucegypt.edu", now, err) == -1,
          "a third request is refused (max 2)");
    CHECK(letters.lettersOf(salma).size() == 2, "Salma has 2 requests");

    printf("Doctor queue (FIFO)\n");
    Vector<LetterRequest> hanyList = letters.pendingFor(hany);
    CHECK(hanyList.size() == 2, "Dr. Hany has 2 pending requests");
    CHECK(hanyList.size() == 2 && hanyList[0].studentEmail == salma, "Salma asked first, so she is first");
    CHECK(letters.pendingFor(hany).size() == 2, "reading the queue does not lose anyone");

    printf("Answering\n");
    CHECK(!letters.hasReceivedLetter(salma), "Salma has no letter yet");
    CHECK(!letters.answer(first, rana, true, "text", now, err), "another doctor cannot answer");
    CHECK(!letters.answer(first, hany, true, "", now, err), "an empty letter cannot be sent");
    CHECK(!letters.answer(999, hany, true, "text", now, err), "an unknown id is refused");
    CHECK(letters.answer(second, hany, true, "Omar is excellent.", now, err), "Dr. Hany answers Omar first");
    hanyList = letters.pendingFor(hany);
    CHECK(hanyList.size() == 1 && hanyList[0].studentEmail == salma, "only Salma is left in his queue");
    CHECK(letters.answer(first, hany, true, "Salma is hard-working.", now, err), "Dr. Hany answers Salma");
    CHECK(letters.pendingFor(hany).size() == 0, "his queue is empty");
    CHECK(!letters.answer(first, hany, true, "again", now, err), "he cannot answer twice");
    CHECK(letters.hasReceivedLetter(salma), "Salma has her letter");
    CHECK(letters.answer(third, rana, false, "", now, err), "Dr. Rana declines");
    CHECK(letters.hasReceivedLetter(omar), "Omar has his letter");

    printf("Restart (loadAll)\n");
    LetterService restarted(repo);
    restarted.loadAll();
    CHECK(restarted.lettersOf(salma).size() == 2, "Salma's 2 requests are loaded back");
    CHECK(restarted.hasReceivedLetter(salma), "her letter is still Received");
    CHECK(restarted.pendingFor(hany).size() == 0, "Dr. Hany's queue is still empty");
    CHECK(restarted.pendingFor(rana).size() == 0, "Dr. Rana's declined request is not pending");

    sqlite3_close(db);
    printf("%s (%d failed)\n", failures == 0 ? "ALL PASSED" : "SOME FAILED", failures);
    return failures == 0 ? 0 : 1;
}
