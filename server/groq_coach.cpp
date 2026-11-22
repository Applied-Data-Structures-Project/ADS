// groq_coach.cpp
// Sends one request to Groq's chat API:
//   system message = the coach's instructions and the question's criteria
//   user message   = the student's essay
// and returns the AI's reply.

#include "groq_coach.hpp"

#include <cstdlib>
#include "crow.h"   // only for crow::json, to build and read the JSON

#ifndef ADS_NO_CURL
#include <curl/curl.h>
#endif

using namespace std;

static string envOr(const char* name, const string& fallback) {
    const char* value = getenv(name);
    return (value && *value) ? string(value) : fallback;
}

GroqCoach::GroqCoach() {
    key = envOr("GROQ_API_KEY", "");
        model = envOr("GROQ_MODEL", "openai/gpt-oss-20b");
    url = envOr("GROQ_URL", "https://api.groq.com/openai/v1/chat/completions");
}
bool GroqCoach::configured() const {
#ifdef ADS_NO_CURL
    return false;
#else
    return !key.empty();
#endif
}
#ifndef ADS_NO_CURL
// libcurl hands us the answer piece by piece; we add each piece to a string.
static size_t keepPart(char* data, size_t size, size_t count, void* userData) {
    static_cast<string*>(userData)->append(data, size * count);
    return size * count;
}
#endif
bool GroqCoach::ask(const string& instructions, const string& essay, string& feedback,
                    string& error) {
    if (!configured()) {
        error = "The AI coach is not set up on this server.";
        return false;
    }
#ifdef ADS_NO_CURL
    (void)instructions; (void)essay; (void)feedback;
    return false;
#else
    // 1. Build the request JSON
    crow::json::wvalue request;
    request["model"] = model;
        request["temperature"] = 0.3;          // calm, consistent feedback
    request["max_tokens"] = 1500;          // room for its thinking + a short answer
    if (model.find("gpt-oss") != string::npos)
        request["reasoning_effort"] = "low";   // think briefly: faster, fewer tokens
    request["messages"][0]["role"] = "system";
    request["messages"][0]["content"] = instructions;
    request["messages"][1]["role"] = "user";
    request["messages"][1]["content"] = essay;
    string body = request.dump();
        // 2. Send it
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        error = "The AI coach is not available right now.";
        return false;
    }
    string answer;
    string auth = "Authorization: Bearer " + key;
    struct curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, keepPart);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &answer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
        if (result != CURLE_OK) {
        CROW_LOG_ERROR << "Groq request failed: " << curl_easy_strerror(result);
        error = "The AI coach is not available right now. Please try again.";
        return false;
    }
    if (status == 429) {
        error = "The AI coach is busy (daily limit reached). Please try again later.";
        return false;
    }

    // 3. Read the reply: choices[0].message.content
    auto json = crow::json::load(answer);
    if (status != 200 || !json || !json.has("choices") || json["choices"].size() == 0) {
        CROW_LOG_ERROR << "Groq answered " << status << ": " << answer.substr(0, 300);
        error = "The AI coach is not available right now. Please try again.";
        return false;
    }
     const auto& message = json["choices"][0]["message"];
    if (!message.has("content") || message["content"].t() != crow::json::type::String ||
        message["content"].s().size() == 0) {
        CROW_LOG_ERROR << "Groq answered with no text: " << answer.substr(0, 300);
        error = "The AI coach is not available right now. Please try again.";
        return false;
    }
    feedback = string(message["content"].s());
    return true; 
#endif
}