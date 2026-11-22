// smtp_notifier.cpp
// Forwards outgoing emails over plain localhost HTTP (standalone Asio) to SvelteKit's
// Node.js relay (/api/internal/email), which handles SMTP + TLS portably on every OS.

#include "smtp_notifier.hpp"

#include <asio.hpp>
#include "crow.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace std;

static string envOr(const char* name, const string& fallback) {
    const char* value = getenv(name);
    return (value && *value) ? string(value) : fallback;
}

static uint32_t rotr32(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32U - n));
}

// Standard FIPS 180-4 SHA-256 returning lowercase 64-character hex.
static string sha256Hex(const string& input) {
    static const uint32_t k[64] = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
    };

    uint32_t h[8] = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
    };

    vector<uint8_t> bytes(input.begin(), input.end());
    uint64_t bitLen = static_cast<uint64_t>(bytes.size()) * 8ULL;
    bytes.push_back(0x80U);
    while ((bytes.size() % 64U) != 56U) {
        bytes.push_back(0x00U);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<uint8_t>((bitLen >> shift) & 0xffU));
    }

    for (size_t chunk = 0; chunk < bytes.size(); chunk += 64U) {
        uint32_t w[64];
        for (size_t i = 0; i < 16U; i++) {
            size_t idx = chunk + i * 4U;
            w[i] = (static_cast<uint32_t>(bytes[idx]) << 24) |
                   (static_cast<uint32_t>(bytes[idx + 1U]) << 16) |
                   (static_cast<uint32_t>(bytes[idx + 2U]) << 8) |
                   static_cast<uint32_t>(bytes[idx + 3U]);
        }
        for (size_t i = 16; i < 64U; i++) {
            uint32_t s0 = rotr32(w[i - 15], 7U) ^ rotr32(w[i - 15], 18U) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr32(w[i - 2], 17U) ^ rotr32(w[i - 2], 19U) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (size_t i = 0; i < 64U; i++) {
            uint32_t S1 = rotr32(e, 6U) ^ rotr32(e, 11U) ^ rotr32(e, 25U);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = rotr32(a, 2U) ^ rotr32(a, 13U) ^ rotr32(a, 22U);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;

            hh = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    char hex[65];
    for (int i = 0; i < 8; i++) {
        snprintf(hex + (i * 8), 9, "%08x", h[i]);
    }
    hex[64] = '\0';
    return string(hex);
}

static string computeRelayToken(const string& secret, const string& user, const string& pass) {
    if (!secret.empty()) {
        return sha256Hex("ads-email-relay-secret:" + secret);
    }
    if (!user.empty()) {
        return sha256Hex("ads-email-relay:" + user + ":" + pass);
    }
    return "";
}

static bool parseHttpUrl(const string& url, string& host, string& port, string& path) {
    const string prefix = "http://";
    if (url.compare(0, prefix.size(), prefix) != 0) return false;

    size_t authStart = prefix.size();
    size_t slash = url.find('/', authStart);
    string authority = (slash == string::npos) ? url.substr(authStart) : url.substr(authStart, slash - authStart);
    path = (slash == string::npos) ? "/" : url.substr(slash);

    if (authority.empty()) return false;

    if (authority.front() == '[') {
        size_t closeBracket = authority.find(']');
        if (closeBracket == string::npos) return false;
        host = authority.substr(1, closeBracket - 1);
        if (closeBracket + 1 < authority.size()) {
            if (authority[closeBracket + 1] != ':') return false;
            port = authority.substr(closeBracket + 2);
        } else {
            port = "80";
        }
    } else {
        size_t colon = authority.find(':');
        if (colon == string::npos) {
            host = authority;
            port = "80";
        } else {
            host = authority.substr(0, colon);
            port = authority.substr(colon + 1);
        }
    }

    return !host.empty() && !port.empty();
}

static int parseHttpStatusCode(const string& response) {
    size_t lineEnd = response.find("\r\n");
    if (lineEnd == string::npos) lineEnd = response.find('\n');
    string statusLine = (lineEnd == string::npos) ? response : response.substr(0, lineEnd);
    if (statusLine.compare(0, 5, "HTTP/") != 0) return 0;
    size_t space = statusLine.find(' ');
    if (space == string::npos) return 0;
    return atoi(statusLine.c_str() + space + 1);
}

static void printFallbackEmail(const string& reason, const string& to,
                               const string& subject, const string& body) {
    printf("\n--- email (not sent: %s) ---\nTo: %s\nSubject: %s\n\n%s---\n",
           reason.c_str(), to.c_str(), subject.c_str(), body.c_str());
}

SmtpNotifier::SmtpNotifier() {
    user = envOr("SMTP_USER", "");
    pass = envOr("SMTP_PASS", "");
    relaySecret = envOr("EMAIL_RELAY_SECRET", "");

    string siteUrl = envOr("SITE_URL", "http://localhost:5173");
    while (!siteUrl.empty() && siteUrl.back() == '/') {
        siteUrl.pop_back();
    }
    string defaultRelay = siteUrl + "/api/internal/email";
    relayUrl = envOr("EMAIL_RELAY_URL", defaultRelay);
}

bool SmtpNotifier::send(const string& to, const string& subject, const string& body) {
    if (!configured()) {
        printFallbackEmail("SMTP_USER is not set", to, subject, body);
        return true;
    }

    string host, port, path;
    if (!parseHttpUrl(relayUrl, host, port, path)) {
        fprintf(stderr, "Invalid EMAIL_RELAY_URL (must use http://): %s\n", relayUrl.c_str());
        printFallbackEmail("invalid email relay URL", to, subject, body);
        return false;
    }

    crow::json::wvalue payload;
    payload["to"] = to;
    payload["subject"] = subject;
    payload["body"] = body;
    string bodyJson = payload.dump();

    string hostHeader = (host.find(':') != string::npos) ? ("[" + host + "]:" + port) : (host + ":" + port);
    string token = computeRelayToken(relaySecret, user, pass);

    string httpReq =
        "POST " + path + " HTTP/1.1\r\n"
        "Host: " + hostHeader + "\r\n"
        "Content-Type: application/json\r\n"
        "X-Internal-Email-Token: " + token + "\r\n"
        "Content-Length: " + to_string(bodyJson.size()) + "\r\n"
        "Connection: close\r\n\r\n" +
        bodyJson;

    asio::io_context io;
    asio::ip::tcp::resolver resolver(io);
    asio::ip::tcp::socket socket(io);
    asio::steady_timer timer(io);

    bool done = false;
    bool timedOut = false;
    bool connectFailed = false;
    string response;
    asio::error_code finalEc;

    timer.expires_after(chrono::seconds(10));
    timer.async_wait([&](const asio::error_code& ec) {
        if (!ec && !done) {
            timedOut = true;
            asio::error_code ignored;
            socket.close(ignored);
            resolver.cancel();
        }
    });

    resolver.async_resolve(host, port, [&](const asio::error_code& ec, asio::ip::tcp::resolver::results_type endpoints) {
        if (ec) {
            connectFailed = !timedOut;
            finalEc = ec;
            done = true;
            timer.cancel();
            return;
        }
        asio::async_connect(socket, endpoints, [&](const asio::error_code& cec, const asio::ip::tcp::endpoint&) {
            if (cec) {
                connectFailed = !timedOut;
                finalEc = cec;
                done = true;
                timer.cancel();
                return;
            }
            asio::async_write(socket, asio::buffer(httpReq), [&](const asio::error_code& wec, size_t) {
                if (wec) {
                    finalEc = wec;
                    done = true;
                    timer.cancel();
                    return;
                }
                asio::async_read(socket, asio::dynamic_buffer(response), asio::transfer_all(),
                                 [&](const asio::error_code& rec, size_t) {
                                     if (rec && rec != asio::error::eof) {
                                         finalEc = rec;
                                     }
                                     done = true;
                                     timer.cancel();
                                     asio::error_code ignored;
                                     socket.close(ignored);
                                 });
            });
        });
    });

    io.run();

    if (timedOut) {
        fprintf(stderr, "Email to %s failed: email relay timed out (%s)\n", to.c_str(), relayUrl.c_str());
        return false;
    }
    if (connectFailed) {
        fprintf(stderr, "Email to %s not sent: relay unreachable at %s (%s)\n",
                to.c_str(), relayUrl.c_str(), finalEc.message().c_str());
        printFallbackEmail("email relay unreachable at " + relayUrl, to, subject, body);
        return false;
    }
    if (finalEc) {
        fprintf(stderr, "Email to %s failed: %s\n", to.c_str(), finalEc.message().c_str());
        return false;
    }

    int status = parseHttpStatusCode(response);
    if (status >= 200 && status < 300) {
        return true;
    }

    fprintf(stderr, "Email to %s failed: relay returned HTTP %d\n", to.c_str(), status);
    return false;
}
