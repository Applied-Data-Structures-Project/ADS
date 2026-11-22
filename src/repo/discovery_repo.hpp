#pragma once

// discovery_repo.hpp
// The only file that runs SQL on `saved_universities` and `notifications`.

#include <string>
#include "sqlite3.h"
#include "vector.hpp"
#include "models/discovery.hpp"

class DiscoveryRepo {
public:
    explicit DiscoveryRepo(sqlite3* db);

    Vector<SavedUniversity> loadSaved();
    bool addSaved(const SavedUniversity& item);
    bool removeSaved(const std::string& studentEmail, const std::string& universitySlug);

    Vector<Notification> loadNotifications();
    int insertNotification(const Notification& n);
    bool markNotificationRead(int id);
    bool markAllNotificationsRead(const std::string& studentEmail);

private:
    sqlite3* db;
};
