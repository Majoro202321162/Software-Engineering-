// Time helpers. All times are held as UTC time points and stored as
// ISO-8601 text such as 2026-11-02T14:30:00Z (DD-08). Local time is used
// only for display and for reading dates typed by users.
#pragma once
#include <chrono>
#include <optional>
#include <string>

namespace lms {

using TimePoint = std::chrono::system_clock::time_point;

std::string toIso(TimePoint t);                         // "2026-11-02T14:30:00Z"
TimePoint fromIso(const std::string& text);             // throws ValidationException
std::string toLocalDisplay(TimePoint t);                // "2026-11-02 16:30" (local time)
std::optional<TimePoint> parseLocal(const std::string& text); // "YYYY-MM-DD HH:MM" local -> UTC

} // namespace lms
