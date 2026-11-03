// Submission life cycle as a transition table (DD-06, Table 4.1, FR-28).
#pragma once
#include "domain/Types.h"

namespace lms {

class SubmissionLifecycle {
public:
    static bool isAllowed(SubmissionStatus from, SubmissionStatus to);
    // Throws BusinessRuleException with a plain-language message when not allowed.
    static void require(SubmissionStatus from, SubmissionStatus to);
};

// What a student may see for each status (Table 4.2, FR-27, BR-02).
std::string studentStatusText(SubmissionStatus s);
// Plain label for staff screens.
std::string staffStatusText(SubmissionStatus s);

} // namespace lms
