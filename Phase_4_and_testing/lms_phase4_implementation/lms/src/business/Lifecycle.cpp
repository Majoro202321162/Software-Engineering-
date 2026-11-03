#include "business/Lifecycle.h"
#include "common/Exceptions.h"

#include <utility>

namespace lms {
namespace {
using S = SubmissionStatus;
// T3 .. T12 (T1 and T2 create the submission and have no "from" status).
const std::pair<S, S> kAllowed[] = {
    {S::SUBMITTED, S::SUBMITTED},  // T3 resubmit (new version)
    {S::LATE, S::SUBMITTED},       // T4 extension covers the submission
    {S::SUBMITTED, S::MARKED},     // T5
    {S::LATE, S::MARKED},          // T6
    {S::MARKED, S::MARKED},        // T7 re-mark
    {S::MARKED, S::MODERATED},     // T8 approve / adjust
    {S::MARKED, S::RETURNED},      // T9 HoD returns
    {S::RETURNED, S::MARKED},      // T10 re-mark after return
    {S::MODERATED, S::RELEASED},   // T11 release
    {S::RELEASED, S::RELEASED},    // T12 post-release correction
};
} // namespace

bool SubmissionLifecycle::isAllowed(SubmissionStatus from, SubmissionStatus to) {
    for (const auto& t : kAllowed)
        if (t.first == from && t.second == to) return true;
    return false;
}

void SubmissionLifecycle::require(SubmissionStatus from, SubmissionStatus to) {
    if (isAllowed(from, to)) return;
    std::string why;
    if (to == S::RELEASED) why = "Only moderated marks can be released.";
    else if (from == S::MODERATED || from == S::RELEASED)
        why = "A moderated or released mark can no longer be changed by the lecturer.";
    else if (from == S::LATE && to == S::SUBMITTED) why = "A late submission cannot be resubmitted.";
    else why = "That change is not allowed in the submission life cycle.";
    throw BusinessRuleException("A " + staffStatusText(from) + " submission cannot become " +
                                staffStatusText(to) + ". " + why);
}

std::string studentStatusText(SubmissionStatus s) {
    switch (s) {
        case S::SUBMITTED: return "Submitted";
        case S::LATE: return "Late";
        case S::RELEASED: return "Released";
        default: return "Awaiting release";
    }
}

std::string staffStatusText(SubmissionStatus s) {
    switch (s) {
        case S::SUBMITTED: return "Submitted";
        case S::LATE: return "Late";
        case S::MARKED: return "Marked";
        case S::RETURNED: return "Returned";
        case S::MODERATED: return "Moderated";
        case S::RELEASED: return "Released";
    }
    return "?";
}

} // namespace lms
