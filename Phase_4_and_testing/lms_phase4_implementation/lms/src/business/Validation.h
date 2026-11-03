// Small validation helpers used by the services (second line of defence, Table 2.4).
#pragma once
#include "common/Exceptions.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>

namespace lms {

inline std::string trim(const std::string& s) {
    auto b = s.begin();
    auto e = s.end();
    while (b != e && std::isspace(static_cast<unsigned char>(*b))) ++b;
    while (e != b && std::isspace(static_cast<unsigned char>(*(e - 1)))) --e;
    return std::string(b, e);
}

inline void requireLength(const std::string& field, const std::string& value, size_t minLen,
                          size_t maxLen) {
    if (value.size() < minLen || value.size() > maxLen)
        throw ValidationException(field + " must be " + std::to_string(minLen) + " to " +
                                  std::to_string(maxLen) + " characters long.");
}

inline double round2(double x) { return std::floor(x * 100.0 + 0.5) / 100.0; }

// BR-14: 0 to maximum in steps of 0.5.
inline void requireMarkValue(const std::string& field, double v, int maxMark) {
    if (!std::isfinite(v) || v < 0 || v > maxMark)
        throw ValidationException(field + " must be between 0 and " + std::to_string(maxMark) + ".");
    if (std::fabs(v * 2.0 - std::floor(v * 2.0 + 0.5)) > 1e-9)
        throw ValidationException(field + " must be a multiple of 0.5.");
}

// Usernames: 3 to 30 letters, digits, dot, dash or underscore.
inline void requireUsername(const std::string& u) {
    requireLength("Username", u, 3, 30);
    for (unsigned char c : u)
        if (!(std::isalnum(c) || c == '.' || c == '_' || c == '-'))
            throw ValidationException("Username may contain only letters, digits, '.', '_' and '-'.");
}

// 80 -> "80", 72.5 -> "72.5", 6.3 -> "6.3" (used for audit values and screens).
inline std::string formatMark(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", v);
    std::string s = buf;
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s.empty() ? "0" : s;
}

} // namespace lms
