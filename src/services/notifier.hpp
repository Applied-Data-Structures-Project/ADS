#pragma once

// notifier.hpp
// Sending emails, kept behind a small interface so the services stay testable:
//   - the real server uses SmtpNotifier (server/smtp_notifier.cpp), which really sends
//   - tests use FakeNotifier below, which only remembers what would have been sent

#include <string>
#include "vector.hpp"

struct Email {
    std::string to;
    std::string subject;
    std::string body;
};

class Notifier {
public:
    virtual ~Notifier() {}
    // Returns true if the email was sent.
    virtual bool send(const std::string& to, const std::string& subject, const std::string& body) = 0;
};

// For tests: nothing leaves the computer, the emails are kept in a list.
class FakeNotifier : public Notifier {
public:
    Vector<Email> sent;

    bool send(const std::string& to, const std::string& subject, const std::string& body) override {
        Email e;
        e.to = to;
        e.subject = subject;
        e.body = body;
        sent.push_back(e);
        return true;
    }
};