// Validated prompts (Phase 3, Table 2.3): type, range, length and allowed values are checked
// here, and the prompt repeats on error (FR-31). Contains no business rules and no SQL.
#pragma once
#include "common/TimeUtil.h"

#include <functional>
#include <iosfwd>
#include <optional>
#include <string>

namespace lms {

class InputReader {
public:
    InputReader(std::istream& in, std::ostream& out) : in_(in), out_(out) {}

    // Called after every line read; used for the 10-minute inactivity check (FR-05).
    void setActivityHook(std::function<void()> hook) { hook_ = std::move(hook); }
    void clearActivityHook() { hook_ = nullptr; }

    std::string readLine(const std::string& prompt);                    // throws EndOfInput
    std::string readText(const std::string& prompt, size_t minLen, size_t maxLen);
    std::string readOptionalText(const std::string& prompt, size_t maxLen);   // empty allowed
    int readInt(const std::string& prompt, int lo, int hi);
    std::optional<int> readId(const std::string& prompt);              // 0 = cancel
    double readMark(const std::string& prompt, int maxMark);           // 0 to max, steps of 0.5
    bool confirm(const std::string& prompt);                           // yes / no
    std::string readPassword(const std::string& prompt);               // no echo on a terminal
    TimePoint readDateTime(const std::string& prompt);                 // "YYYY-MM-DD HH:MM" local time

private:
    std::istream& in_;
    std::ostream& out_;
    std::function<void()> hook_;
};

} // namespace lms
