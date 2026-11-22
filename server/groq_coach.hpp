#pragma once

// groq_coach.hpp
// Asks an AI (Groq) for feedback on one essay. Lives in server/ because it needs the
// internet (libcurl), like smtp_notifier.
// Settings come from environment variables (never put the key in the code):
#include <string>

class GroqCoach {
public:
    GroqCoach();   // reads the environment variables
    bool configured() const;

    // instructions = what the coach must check; essay = the student's text.
    // Returns true and fills feedback, or false and fills error.
    bool ask(const std::string& instructions, const std::string& essay,
             std::string& feedback, std::string& error);

private:
    std::string key;
    std::string model;
    std::string url;
};