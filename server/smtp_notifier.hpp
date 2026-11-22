#pragma once

// smtp_notifier.hpp
// Group 3 - Malak
//
// Sends real emails via the SvelteKit Node.js email relay (POST /api/internal/email),
// avoiding any C++ TLS / libcurl OS dependencies on Windows, macOS, and Linux.
// Lives in server/, not src/, so the services and tests stay isolated from HTTP.
//
// Settings come from environment variables (never put passwords in the code):
//   SMTP_USER           the sending account, e.g. aucexchange.demo@gmail.com
//   SMTP_PASS           its app password
//   SITE_URL            base URL of the SvelteKit server (default http://localhost:5173)
//   EMAIL_RELAY_URL     optional override for <SITE_URL>/api/internal/email
//   EMAIL_RELAY_SECRET  optional shared secret (defaults to hash derived from SMTP credentials)
// If SMTP_USER is empty (or the frontend server is offline), emails are printed in the terminal.

#include <string>
#include "services/notifier.hpp"

class SmtpNotifier : public Notifier {
public:
    SmtpNotifier();   // reads the environment variables
    bool send(const std::string& to, const std::string& subject, const std::string& body) override;
    bool configured() const { return !user.empty(); }

private:
    std::string relayUrl;
    std::string user;
    std::string pass;
    std::string relaySecret;
};
