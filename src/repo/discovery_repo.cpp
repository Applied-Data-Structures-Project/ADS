// discovery_repo.cpp
// Loads and saves rows in `saved_universities` and `notifications`.

#include "repo/discovery_repo.hpp"

using namespace std;

static string colText(sqlite3_stmt* s, int i) {
    const unsigned char* t = sqlite3_column_text(s, i);
    return t ? reinterpret_cast<const char*>(t) : "";
}

static int colInt(sqlite3_stmt* s, int i) {
    if (sqlite3_column_type(s, i) == SQLITE_NULL) return 0;
    return sqlite3_column_int(s, i);
}

DiscoveryRepo::DiscoveryRepo(sqlite3* db) : db(db) {}

// ---------- saved_universities ----------

Vector<SavedUniversity> DiscoveryRepo::loadSaved() {
    Vector<SavedUniversity> result;
    const char* sql =
        "SELECT student_email, university_slug, saved_at "
        "FROM saved_universities "
        "ORDER BY saved_at ASC, rowid ASC";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SavedUniversity item;
        item.studentEmail = colText(stmt, 0);
        item.universitySlug = colText(stmt, 1);
        item.savedAt = colText(stmt, 2);
        result.push_back(item);
    }

    sqlite3_finalize(stmt);
    return result;
}

bool DiscoveryRepo::addSaved(const SavedUniversity& item) {
    const char* sql =
        "INSERT OR IGNORE INTO saved_universities (student_email, university_slug, saved_at) "
        "VALUES (?, ?, ?)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, item.studentEmail.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, item.universitySlug.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, item.savedAt.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool DiscoveryRepo::removeSaved(const string& studentEmail, const string& universitySlug) {
    const char* sql =
        "DELETE FROM saved_universities WHERE student_email = ? AND university_slug = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, studentEmail.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, universitySlug.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

// ---------- notifications ----------

Vector<Notification> DiscoveryRepo::loadNotifications() {
    Vector<Notification> result;
    const char* sql =
        "SELECT id, student_email, message, created_at, is_read "
        "FROM notifications "
        "ORDER BY id ASC";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Notification n;
        n.id = colInt(stmt, 0);
        n.studentEmail = colText(stmt, 1);
        n.message = colText(stmt, 2);
        n.createdAt = colText(stmt, 3);
        n.read = colInt(stmt, 4) == 1;
        result.push_back(n);
    }

    sqlite3_finalize(stmt);
    return result;
}

int DiscoveryRepo::insertNotification(const Notification& n) {
    const char* sql =
        "INSERT INTO notifications (student_email, message, created_at, is_read) "
        "VALUES (?, ?, ?, ?)";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, n.studentEmail.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, n.message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, n.createdAt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, n.read ? 1 : 0);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    if (!ok) return -1;
    return static_cast<int>(sqlite3_last_insert_rowid(db));
}

bool DiscoveryRepo::markNotificationRead(int id) {
    const char* sql = "UPDATE notifications SET is_read = 1 WHERE id = ?";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int(stmt, 1, id);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool DiscoveryRepo::markAllNotificationsRead(const string& studentEmail) {
    const char* sql = "UPDATE notifications SET is_read = 1 WHERE student_email = ? AND is_read = 0";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, studentEmail.c_str(), -1, SQLITE_TRANSIENT);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}
