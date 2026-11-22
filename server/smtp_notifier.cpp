// smtp_notifier.cpp
// An email is plain text: a few header lines (From, To, Subject...), an empty line, then the body.
// libcurl connects to the mail server, logs in, and uploads that text.

#include "smtp_notifier.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#ifndef ADS_NO_CURL
#include <curl/curl.h>
#endif

using namespace std;

static string envOr(const char* name, const string& fallback) {
    const char* value = getenv(name);
    return (value && *value) ? string(value) : fallback;
}

SmtpNotifier::SmtpNotifier() {
    url = envOr("SMTP_URL", "smtps://smtp.gmail.com:465");
    user = envOr("SMTP_USER", "");
    pass = envOr("SMTP_PASS", "");
    from = envOr("SMTP_FROM", user);
}

#ifndef ADS_NO_CURL
// Emails need "\r\n" at the end of every line.
static string withCrlf(const string& text) {
    string out;
    for (char c : text) {
        if (c == '\n') out += "\r\n";
        else out += c;
    }
    return out;
}

static string dateHeader() {
    char buf[64];
    time_t t = time(nullptr);
    strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %z", localtime(&t));
    return buf;
}

// libcurl asks for the email piece by piece; we hand it the next part of our text each time.
struct Upload {
    string data;
    size_t pos = 0;
};

static size_t readPart(char* buffer, size_t size, size_t count, void* userData) {
    Upload* up = static_cast<Upload*>(userData);
    size_t room = size * count;
    size_t left = up->data.size() - up->pos;
    size_t n = left < room ? left : room;
    memcpy(buffer, up->data.data() + up->pos, n);
    up->pos += n;
    return n;   // 0 means "finished"
}
#endif

bool SmtpNotifier::send(const string& to, const string& subject, const string& body) {
    // Not set up (or no libcurl on this computer): show the email in the terminal instead.
#ifdef ADS_NO_CURL
    const char* reason = "this computer has no libcurl";
    bool canSend = false;
#else
    const char* reason = "SMTP_USER is not set";
    bool canSend = configured();
#endif
    if (!canSend) {
        printf("\n--- email (not sent: %s) ---\nTo: %s\nSubject: %s\n\n%s---\n",
               reason, to.c_str(), subject.c_str(), body.c_str());
        return true;
    }

#ifndef ADS_NO_CURL

    Upload up;
    up.data = withCrlf(
        "Date: " + dateHeader() + "\n"
        "To: <" + to + ">\n"
        "From: AUC Exchange Matcher <" + from + ">\n"
        "Subject: " + subject + "\n"
        "MIME-Version: 1.0\n"
        "Content-Type: text/plain; charset=UTF-8\n"
        "\n" + body);

    CURL* curl = curl_easy_init();
    if (curl == nullptr) return false;

    struct curl_slist* recipients = curl_slist_append(nullptr, ("<" + to + ">").c_str());
    string mailFrom = "<" + from + ">";

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERNAME, user.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, pass.c_str());
    // Always encrypted, except a test mail server on this same computer.
    bool local = url.find("://localhost") != string::npos || url.find("://127.0.0.1") != string::npos;
    if (!local) curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, mailFrom.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, readPart);
    curl_easy_setopt(curl, CURLOPT_READDATA, &up);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    string caBundle = envOr("CURL_CA_BUNDLE", envOr("SSL_CERT_FILE", ""));
    if (!caBundle.empty()) curl_easy_setopt(curl, CURLOPT_CAINFO, caBundle.c_str());

    CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        fprintf(stderr, "Email to %s failed: %s\n", to.c_str(), curl_easy_strerror(result));
    }

    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
    return result == CURLE_OK;
#else
    return false;   // never reached: without libcurl we always print above
#endif
}