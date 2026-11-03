#include "common/TimeUtil.h"
#include "common/Exceptions.h"

#include <cstdio>
#include <ctime>

namespace lms {
namespace {

// Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's algorithm).
long long daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<long long>(doe) - 719468;
}

void civilFromDays(long long z, int& y, unsigned& m, unsigned& d) {
    z += 719468;
    const long long era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = static_cast<int>(yoe) + static_cast<int>(era) * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y += (m <= 2);
}

} // namespace

std::string toIso(TimePoint t) {
    const long long secs =
        std::chrono::duration_cast<std::chrono::seconds>(t.time_since_epoch()).count();
    long long days = secs / 86400;
    long long rem = secs % 86400;
    if (rem < 0) { rem += 86400; --days; }
    int y; unsigned m, d;
    civilFromDays(days, y, m, d);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%04d-%02u-%02uT%02lld:%02lld:%02lldZ", y, m, d,
                  rem / 3600, (rem % 3600) / 60, rem % 60);
    return buf;
}

TimePoint fromIso(const std::string& text) {
    int y, mo, d, h, mi, s;
    char tail = 0;
    if (std::sscanf(text.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d%c", &y, &mo, &d, &h, &mi, &s,
                    &tail) != 7 || tail != 'Z' || mo < 1 || mo > 12 || d < 1 || d > 31 ||
        h > 23 || mi > 59 || s > 60) {
        throw ValidationException("Invalid stored time value.");
    }
    const long long days = daysFromCivil(y, static_cast<unsigned>(mo), static_cast<unsigned>(d));
    return TimePoint(std::chrono::seconds(days * 86400 + h * 3600 + mi * 60 + s));
}

std::string toLocalDisplay(TimePoint t) {
    std::time_t tt = std::chrono::system_clock::to_time_t(t);
    std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &tt);
#else
    localtime_r(&tt, &tmv);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &tmv);
    return buf;
}

std::optional<TimePoint> parseLocal(const std::string& text) {
    int y, mo, d, h, mi;
    char extra = 0;
    if (std::sscanf(text.c_str(), "%4d-%2d-%2d %2d:%2d%c", &y, &mo, &d, &h, &mi, &extra) != 5)
        return std::nullopt;
    if (y < 2000 || y > 2100 || mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 ||
        mi < 0 || mi > 59)
        return std::nullopt;
    std::tm tmv{};
    tmv.tm_year = y - 1900; tmv.tm_mon = mo - 1; tmv.tm_mday = d;
    tmv.tm_hour = h; tmv.tm_min = mi; tmv.tm_isdst = -1;
    std::tm check = tmv;
    const std::time_t tt = std::mktime(&tmv);
    if (tt == static_cast<std::time_t>(-1)) return std::nullopt;
    if (tmv.tm_mday != check.tm_mday || tmv.tm_mon != check.tm_mon) return std::nullopt; // e.g. 31 Feb
    return std::chrono::system_clock::from_time_t(tt);
}

} // namespace lms
