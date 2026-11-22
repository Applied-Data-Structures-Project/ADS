#pragma once

// discovery_service.hpp
// Manages each student's saved universities and FIFO notification queue.
// Data structures (no STL containers):
//   HashMap<email, Vector<slug>>   saved universities in save order
//   HashMap<"email|slug", bool>    O(1) saved membership check
//   HashMap<id, Notification>      O(1) notification lookup
//   HashMap<email, Vector<id>>     all notification IDs for a student (oldest first)
//   HashMap<email, Queue<id>>      FIFO queue of unread notification IDs

#include <string>
#include "hashmap.hpp"
#include "queue.hpp"
#include "vector.hpp"
#include "models/discovery.hpp"
#include "repo/discovery_repo.hpp"

class DiscoveryService {
public:
    explicit DiscoveryService(DiscoveryRepo& repo);

    // Load saved universities and notifications from SQLite into memory at startup.
    void loadAll();

    // ---- Saved universities ----
    Vector<std::string> savedOf(const std::string& email) const;
    bool isSaved(const std::string& email, const std::string& slug) const;
    bool saveUniversity(const std::string& email, const std::string& slug,
                        const std::string& now, std::string& error);
    bool removeUniversity(const std::string& email, const std::string& slug,
                          std::string& error);

    // ---- Notifications (FIFO queue) ----
    Vector<Notification> notificationsOf(const std::string& email) const;
    int unreadCount(const std::string& email) const;
    int notify(const std::string& email, const std::string& message,
               const std::string& now, std::string& error);

    // Marks the oldest unread notification (front of the FIFO queue) as read.
    // Returns the notification ID marked read, 0 if none are unread, or -1 on error.
    int markOldestRead(const std::string& email, std::string& error);

    // Marks every unread notification for this student as read and empties the queue.
    bool markAllRead(const std::string& email, std::string& error);

    // Seeds one welcome notification the first time a student has no notifications.
    void ensureWelcomeNotification(const std::string& email, const std::string& now);

private:
    DiscoveryRepo& repo;

    HashMap<std::string, Vector<std::string>> savedByStudent;
    HashMap<std::string, bool> savedIndex;

    HashMap<int, Notification> notificationsById;
    HashMap<std::string, Vector<int>> notificationIdsByStudent;
    HashMap<std::string, Queue<int>> unreadQueueByStudent;

    static std::string savedKey(const std::string& email, const std::string& slug);
};
