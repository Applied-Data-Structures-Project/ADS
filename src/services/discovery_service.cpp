// discovery_service.cpp
// Saved universities and FIFO notification queue backed by SQLite.

#include "services/discovery_service.hpp"
#include "mergesort.hpp"

#include <cctype>

using namespace std;

static string trim(const string& text) {
    size_t start = 0;
    while (start < text.size() && isspace(static_cast<unsigned char>(text[start]))) start++;
    size_t end = text.size();
    while (end > start && isspace(static_cast<unsigned char>(text[end - 1]))) end--;
    return text.substr(start, end - start);
}

static string lower(string text) {
    for (char& c : text) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return text;
}

static bool endsWith(const string& text, const string& suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static bool validSlug(const string& slug) {
    if (slug.empty() || slug.size() > 160) return false;
    for (char c : slug) {
        bool ok = isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_';
        if (!ok) return false;
    }
    return true;
}

DiscoveryService::DiscoveryService(DiscoveryRepo& repo) : repo(repo) {}

string DiscoveryService::savedKey(const string& email, const string& slug) {
    return email + "|" + slug;
}

void DiscoveryService::loadAll() {
    Vector<SavedUniversity> saved = repo.loadSaved();
    for (int i = 0; i < saved.size(); i++) {
        string email = lower(saved[i].studentEmail);
        string slug = lower(saved[i].universitySlug);
        string key = savedKey(email, slug);
        if (!savedIndex.search(key)) {
            savedIndex.insert(key, true);
            savedByStudent[email].push_back(slug);
        }
    }

    Vector<Notification> notes = repo.loadNotifications();
    for (int i = 0; i < notes.size(); i++) {
        Notification n = notes[i];
        n.studentEmail = lower(n.studentEmail);
        notificationsById.insert(n.id, n);
        notificationIdsByStudent[n.studentEmail].push_back(n.id);
        if (!n.read) {
            unreadQueueByStudent[n.studentEmail].push(n.id);
        }
    }
}

// ---------- Saved universities ----------

Vector<string> DiscoveryService::savedOf(const string& email) const {
    const Vector<string>* list = savedByStudent.get(lower(email));
    if (list == nullptr) return Vector<string>();
    return *list;
}

bool DiscoveryService::isSaved(const string& email, const string& slug) const {
    return savedIndex.search(savedKey(lower(email), lower(slug)));
}

bool DiscoveryService::saveUniversity(const string& rawEmail, const string& rawSlug,
                                      const string& now, string& error) {
    string email = lower(trim(rawEmail));
    string slug = lower(trim(rawSlug));

    if (email.empty() || !endsWith(email, "@aucegypt.edu")) {
        error = "Sign in with your AUC account.";
        return false;
    }
    if (!validSlug(slug)) {
        error = "Invalid university slug.";
        return false;
    }

    string key = savedKey(email, slug);
    if (savedIndex.search(key)) return true;   // already saved (idempotent)

    SavedUniversity item;
    item.studentEmail = email;
    item.universitySlug = slug;
    item.savedAt = now;
    if (!repo.addSaved(item)) {
        error = "Could not save university to the database.";
        return false;
    }

    savedIndex.insert(key, true);
    savedByStudent[email].push_back(slug);
    return true;
}

bool DiscoveryService::removeUniversity(const string& rawEmail, const string& rawSlug,
                                        string& error) {
    string email = lower(trim(rawEmail));
    string slug = lower(trim(rawSlug));

    if (email.empty() || !endsWith(email, "@aucegypt.edu")) {
        error = "Sign in with your AUC account.";
        return false;
    }
    if (!validSlug(slug)) {
        error = "Invalid university slug.";
        return false;
    }

    string key = savedKey(email, slug);
    if (!savedIndex.search(key)) return true;   // not saved (idempotent)

    if (!repo.removeSaved(email, slug)) {
        error = "Could not remove saved university from the database.";
        return false;
    }

    savedIndex.remove(key);
    Vector<string>* current = savedByStudent.get(email);
    if (current != nullptr) {
        Vector<string> filtered;
        for (int i = 0; i < current->size(); i++) {
            if ((*current)[i] != slug) filtered.push_back((*current)[i]);
        }
        *current = filtered;
    }
    return true;
}

// ---------- Notifications (FIFO queue) ----------

Vector<Notification> DiscoveryService::notificationsOf(const string& email) const {
    Vector<Notification> out;
    const Vector<int>* ids = notificationIdsByStudent.get(lower(email));
    if (ids == nullptr) return out;

    for (int i = 0; i < ids->size(); i++) {
        const Notification* n = notificationsById.get((*ids)[i]);
        if (n != nullptr) out.push_back(*n);
    }
    // Guarantee oldest-first FIFO display order using our C++ stable mergeSort.
    mergeSort(out, [](const Notification& a, const Notification& b) {
        if (a.createdAt != b.createdAt) return a.createdAt < b.createdAt;
        return a.id < b.id;
    });
    return out;
}

int DiscoveryService::unreadCount(const string& email) const {
    const Queue<int>* q = unreadQueueByStudent.get(lower(email));
    return q != nullptr ? q->size() : 0;
}

int DiscoveryService::notify(const string& rawEmail, const string& rawMessage,
                             const string& now, string& error) {
    string email = lower(trim(rawEmail));
    string message = trim(rawMessage);

    if (email.empty() || !endsWith(email, "@aucegypt.edu")) {
        error = "Sign in with your AUC account.";
        return -1;
    }
    if (message.empty()) {
        error = "Notification message cannot be empty.";
        return -1;
    }

    Notification n;
    n.studentEmail = email;
    n.message = message;
    n.createdAt = now;
    n.read = false;

    int id = repo.insertNotification(n);
    if (id < 0) {
        error = "Could not save notification to the database.";
        return -1;
    }

    n.id = id;
    notificationsById.insert(id, n);
    notificationIdsByStudent[email].push_back(id);
    unreadQueueByStudent[email].push(id);
    return id;
}

int DiscoveryService::markOldestRead(const string& rawEmail, string& error) {
    string email = lower(trim(rawEmail));
    Queue<int>* q = unreadQueueByStudent.get(email);
    if (q == nullptr || q->empty()) return 0;

    int oldestId = q->front();
    if (!repo.markNotificationRead(oldestId)) {
        error = "Could not update notification in the database.";
        return -1;
    }

    q->pop();
    Notification* n = notificationsById.get(oldestId);
    if (n != nullptr) n->read = true;
    return oldestId;
}

bool DiscoveryService::markAllRead(const string& rawEmail, string& error) {
    string email = lower(trim(rawEmail));
    Queue<int>* q = unreadQueueByStudent.get(email);
    if (q == nullptr || q->empty()) return true;

    if (!repo.markAllNotificationsRead(email)) {
        error = "Could not update notifications in the database.";
        return false;
    }

    while (!q->empty()) {
        int id = q->front();
        q->pop();
        Notification* n = notificationsById.get(id);
        if (n != nullptr) n->read = true;
    }
    return true;
}

void DiscoveryService::ensureWelcomeNotification(const string& rawEmail, const string& now) {
    string email = lower(trim(rawEmail));
    if (email.empty() || !endsWith(email, "@aucegypt.edu")) return;

    const Vector<int>* ids = notificationIdsByStudent.get(email);
    if (ids != nullptr && ids->size() > 0) return;

    string ignored;
    notify(email,
           "Welcome to the AUC exchange portal. Start by exploring partner universities.",
           now, ignored);
}
