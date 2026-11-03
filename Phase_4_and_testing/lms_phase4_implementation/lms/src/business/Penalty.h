// Late-penalty rule behind one interface (Strategy pattern, DD-02, BR-04 to BR-07).
#pragma once
#include "common/TimeUtil.h"

namespace lms {

class IPenaltyStrategy {
public:
    virtual ~IPenaltyStrategy() = default;
    // 0 when not late; otherwise whole days, rounded up (BR-04, BR-05).
    virtual int daysLate(TimePoint submittedAt, TimePoint effectiveDeadline) const = 0;
    // Marks removed for the given number of days (the full maximum once the work counts as zero).
    virtual double penalty(int maxMark, int days) const = 0;
    virtual double finalMark(double rawMark, int maxMark, int days) const = 0;
    // True when the work receives a zero because of its lateness (BR-06).
    virtual bool givesZero(int days) const = 0;
};

// 10% of the maximum mark per day for days 1 to 3; zero from day 4 (BR-06).
class TenPercentPerDayPenalty : public IPenaltyStrategy {
public:
    int daysLate(TimePoint submittedAt, TimePoint effectiveDeadline) const override;
    double penalty(int maxMark, int days) const override;
    double finalMark(double rawMark, int maxMark, int days) const override;
    bool givesZero(int days) const override { return days > lastPenalisedDay_; }
private:
    int ratePercent_ = 10;
    int lastPenalisedDay_ = 3;
};

// For tests: lateness never changes a mark.
class NoPenaltyStrategy : public IPenaltyStrategy {
public:
    int daysLate(TimePoint, TimePoint) const override { return 0; }
    double penalty(int, int) const override { return 0; }
    double finalMark(double raw, int, int) const override { return raw; }
    bool givesZero(int) const override { return false; }
};

} // namespace lms
