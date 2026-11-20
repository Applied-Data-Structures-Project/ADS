#pragma once

// smtp_notifier.hpp
// Group 3 - Malak
//
// Sends real emails with SMTP (through libcurl). Lives in server/, not src/,
// so the services and tests never need an internet connection.
//
// Settings come from environment variables (never put passwords in the code):
//   SMTP_URL   default smtps://smtp.gmail.com:465
//   SMTP_USER  the sending account, e.g. aucexchange.demo@gmail.com
//   SMTP_PASS  its app password
//   SMTP_FROM  optional, defaults to SMTP_USER
// If SMTP_USER is empty, emails are printed in the terminal instead of sent.

#include <string>
#include "services/notifier.hpp"

class SmtpNotifier : public Notifier {
public:
    SmtpNotifier();   // reads the environment variables
    bool send(const std::string& to, const std::string& subject, const std::string& body) override;
    bool configured() const { return !user.empty(); }

private:
    std::string url;
    std::string user;
    std::string pass;
    std::string from;
};
