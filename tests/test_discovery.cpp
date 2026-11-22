// test_discovery.cpp
// Unit tests for DiscoveryRepo and DiscoveryService (saved universities + FIFO notifications).

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "sqlite3.h"
#include "repo/discovery_repo.hpp"
#include "services/discovery_service.hpp"
#include "test_helpers.hpp"

using namespace std;

static sqlite3* openDiscoveryTestDb() {
    sqlite3* db = nullptr;
    sqlite3_open(":memory:", &db);
    ifstream file(projectRoot() + "/db/migrations/004_discovery.sql");
    stringstream sql;
    sql << file.rdbuf();
    if (sqlite3_exec(db, sql.str().c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
        printf("  Could not create discovery tables: %s\n", sqlite3_errmsg(db));
        failures++;
    }
    return db;
}

int main() {
    sqlite3* db = openDiscoveryTestDb();
    DiscoveryRepo repo(db);
    DiscoveryService svc(repo);
    string err;

    printf("Saved universities\n");
    CHECK(svc.savedOf("salma@aucegypt.edu").size() == 0, "starts empty");
    CHECK(svc.saveUniversity("Salma@AUCEgypt.edu", "bocconi-university", "2026-11-22T10:00:00", err),
          "save first university");
    CHECK(svc.saveUniversity("salma@aucegypt.edu", "aarhus-university", "2026-11-22T10:01:00", err),
          "save second university");
    CHECK(svc.saveUniversity("salma@aucegypt.edu", "bocconi-university", "2026-11-22T10:02:00", err),
          "saving duplicate is idempotent");
    CHECK(svc.isSaved("salma@aucegypt.edu", "bocconi-university"), "O(1) isSaved finds saved slug");
    CHECK(!svc.isSaved("salma@aucegypt.edu", "lund-university"), "O(1) isSaved false for unsaved slug");

    Vector<string> saved = svc.savedOf("salma@aucegypt.edu");
    CHECK(saved.size() == 2, "duplicate did not double-insert");
    CHECK(saved.size() == 2 && saved[0] == "bocconi-university" && saved[1] == "aarhus-university",
          "save order preserved");

    CHECK(!svc.saveUniversity("salma@gmail.com", "lund-university", "2026-11-22T10:03:00", err),
          "non-AUC email rejected");
    CHECK(!svc.saveUniversity("salma@aucegypt.edu", "bad slug!", "2026-11-22T10:03:00", err),
          "invalid slug rejected");

    CHECK(svc.removeUniversity("salma@aucegypt.edu", "bocconi-university", err), "remove saved slug");
    CHECK(!svc.isSaved("salma@aucegypt.edu", "bocconi-university"), "removed slug no longer saved");
    saved = svc.savedOf("salma@aucegypt.edu");
    CHECK(saved.size() == 1 && saved[0] == "aarhus-university", "remaining slug preserved");

    printf("Notifications (FIFO queue)\n");
    CHECK(svc.unreadCount("salma@aucegypt.edu") == 0, "starts with 0 unread");
    int id1 = svc.notify("salma@aucegypt.edu", "First notice", "2026-11-22T10:00:00", err);
    int id2 = svc.notify("salma@aucegypt.edu", "Second notice", "2026-11-22T10:05:00", err);
    int id3 = svc.notify("salma@aucegypt.edu", "Third notice", "2026-11-22T10:10:00", err);
    CHECK(id1 > 0 && id2 > id1 && id3 > id2, "autoincrement notification IDs increase");
    CHECK(svc.unreadCount("salma@aucegypt.edu") == 3, "unread queue has 3 items");
    CHECK(svc.notify("salma@aucegypt.edu", "   ", "2026-11-22T10:11:00", err) == -1,
          "empty message rejected");

    int readId = svc.markOldestRead("salma@aucegypt.edu", err);
    CHECK(readId == id1, "markOldestRead dequeues oldest unread first (FIFO)");
    CHECK(svc.unreadCount("salma@aucegypt.edu") == 2, "unread count decremented to 2");

    Vector<Notification> list = svc.notificationsOf("salma@aucegypt.edu");
    CHECK(list.size() == 3 && list[0].read && !list[1].read && !list[2].read,
          "only oldest notification is marked read");

    printf("Welcome notification\n");
    svc.ensureWelcomeNotification("omar@aucegypt.edu", "2026-11-22T10:12:00");
    svc.ensureWelcomeNotification("omar@aucegypt.edu", "2026-11-22T10:13:00");
    CHECK(svc.notificationsOf("omar@aucegypt.edu").size() == 1, "welcome notification seeded once");

    printf("Restart reload (loadAll)\n");
    DiscoveryService fresh(repo);
    fresh.loadAll();
    CHECK(fresh.savedOf("salma@aucegypt.edu").size() == 1 &&
              fresh.isSaved("salma@aucegypt.edu", "aarhus-university"),
          "saved universities survive restart");
    CHECK(fresh.unreadCount("salma@aucegypt.edu") == 2, "unread queue rebuilt with 2 items after restart");
    CHECK(fresh.markOldestRead("salma@aucegypt.edu", err) == id2,
          "rebuilt queue still pops second-oldest next (FIFO)");
    CHECK(fresh.markAllRead("salma@aucegypt.edu", err), "markAllRead succeeds");
    CHECK(fresh.unreadCount("salma@aucegypt.edu") == 0, "markAllRead empties queue");
    CHECK(fresh.markOldestRead("salma@aucegypt.edu", err) == 0, "markOldestRead on empty queue returns 0");

    sqlite3_close(db);
    if (failures > 0) {
        printf("SOME FAILED (%d failed)\n", failures);
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
