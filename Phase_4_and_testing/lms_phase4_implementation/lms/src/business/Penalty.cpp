#include "business/Penalty.h"
#include "business/Validation.h"

#include <algorithm>

namespace lms {

// Exactly at the deadline is on time; one second late is day 1; exactly 24 h late is
// still day 1; 24 h and 1 s is day 2 (Table 3.5).
int TenPercentPerDayPenalty::daysLate(TimePoint submittedAt, TimePoint deadline) const {
    const long long secs =
        std::chrono::duration_cast<std::chrono::seconds>(submittedAt - deadline).count();
    if (secs <= 0) return 0;
    return static_cast<int>((secs + 86399) / 86400);
}

double TenPercentPerDayPenalty::penalty(int maxMark, int days) const {
    if (days <= 0) return 0.0;
    if (days > lastPenalisedDay_) return static_cast<double>(maxMark);
    return round2(static_cast<double>(maxMark) * ratePercent_ * days / 100.0);
}

double TenPercentPerDayPenalty::finalMark(double rawMark, int maxMark, int days) const {
    if (givesZero(days)) return 0.0;
    return std::max(0.0, round2(rawMark - penalty(maxMark, days)));
}

} // namespace lms
