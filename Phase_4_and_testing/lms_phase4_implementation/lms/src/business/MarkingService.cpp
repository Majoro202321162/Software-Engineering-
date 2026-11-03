#include "business/Services.h"

namespace lms {

// Days late, penalty and final mark for the latest version (BR-03 to BR-07).
MarkingService::Outcome MarkingService::assess(const Assignment& a, const Submission& sub, double raw) {
    auto version = r_.submissions.latestVersion(sub.id);
    if (!version) throw BusinessRuleException("That submission has no file.");
    const TimePoint effective = assignments_.effectiveDeadline(a.id, sub.studentId);
    Outcome o;
    o.days = penalty_.daysLate(version->uploadedAt, effective);
    if (penalty_.givesZero(o.days)) {
        o.penalty = raw;                     // the whole raw mark is removed; final mark is zero
        o.finalMark = 0.0;
    } else {
        o.penalty = penalty_.penalty(a.maxMark, o.days);
        o.finalMark = penalty_.finalMark(raw, a.maxMark, o.days);
    }
    return o;
}

// T5 / T6: first marking. Mark, status and audit records are saved in one transaction.
MarkResult MarkingService::mark(const Session& s, int submissionId, double raw, const std::string& rawFeedback) {
    p_.require(s.getUser(), Function::MARK_SUBMISSION);
    Submission sub = scope_.submission(submissionId);
    Assignment a = scope_.assignment(sub.assignmentId);
    scope_.lecturerCourse(s, a.courseId);

    if (sub.status == SubmissionStatus::MARKED || sub.status == SubmissionStatus::RETURNED)
        throw BusinessRuleException("This work is already marked. Use re-mark to change the mark.");
    SubmissionLifecycle::require(sub.status, SubmissionStatus::MARKED);

    const std::string feedback = trim(rawFeedback);
    requireMarkValue("The mark", raw, a.maxMark);
    requireLength("Feedback", feedback, 10, 2000);
    const Outcome o = assess(a, sub, raw);
    const TimePoint now = clock_.now();
    const int me = s.getUser().getId();

    auto tx = r_.uow.begin();
    Mark m;
    m.submissionId = sub.id;
    m.rawMark = raw;
    m.daysLate = o.days;
    m.penalty = o.penalty;
    m.finalMark = o.finalMark;
    m.feedback = feedback;
    m.markedBy = me;
    m.markedAt = now;
    r_.marks.save(m);
    r_.submissions.updateStatus(sub.id, SubmissionStatus::MARKED);
    const std::string why = "Initial marking";
    audit_.recordChange(sub.id, me, AuditAction::INITIAL_MARKING, AuditField::RAW_MARK, std::nullopt, formatMark(raw), why);
    audit_.recordChange(sub.id, me, AuditAction::INITIAL_MARKING, AuditField::FEEDBACK, std::nullopt, feedback, why);
    audit_.recordChange(sub.id, me, AuditAction::INITIAL_MARKING, AuditField::FINAL_MARK, std::nullopt, formatMark(o.finalMark), why);
    tx->commit();
    return MarkResult{raw, o.penalty, o.finalMark, o.days, SubmissionStatus::MARKED};
}

// T7 / T10: change a mark while it is Marked or Returned (FR-22). Reason of 10+ characters.
MarkResult MarkingService::remark(const Session& s, int submissionId, double raw,
                                  const std::string& rawFeedback, const std::string& rawReason) {
    p_.require(s.getUser(), Function::MARK_SUBMISSION);
    Submission sub = scope_.submission(submissionId);
    Assignment a = scope_.assignment(sub.assignmentId);
    scope_.lecturerCourse(s, a.courseId);

    if (sub.status == SubmissionStatus::SUBMITTED || sub.status == SubmissionStatus::LATE)
        throw BusinessRuleException("This work has not been marked yet.");
    SubmissionLifecycle::require(sub.status, SubmissionStatus::MARKED);   // refuses Moderated / Released

    const std::string feedback = trim(rawFeedback);
    const std::string reason = trim(rawReason);
    requireMarkValue("The mark", raw, a.maxMark);
    requireLength("Feedback", feedback, 10, 2000);
    requireLength("Reason", reason, 10, 500);

    auto existing = r_.marks.find(sub.id);
    if (!existing) throw BusinessRuleException("The mark could not be found.");
    const bool rawChanged = std::fabs(existing->rawMark - raw) > 1e-9;
    const bool feedbackChanged = existing->feedback != feedback;
    if (!rawChanged && !feedbackChanged) throw ValidationException("Nothing was changed.");

    const Outcome o = assess(a, sub, raw);          // an extension may have changed the penalty (BR-09)
    const bool finalChanged = std::fabs(existing->finalMark - o.finalMark) > 1e-9;
    const int me = s.getUser().getId();
    const Mark before = *existing;

    auto tx = r_.uow.begin();
    Mark m = before;
    m.rawMark = raw;
    m.daysLate = o.days;
    m.penalty = o.penalty;
    m.finalMark = o.finalMark;
    m.feedback = feedback;
    m.markedBy = me;
    m.markedAt = clock_.now();
    r_.marks.save(m);
    r_.submissions.updateStatus(sub.id, SubmissionStatus::MARKED);
    if (rawChanged)
        audit_.recordChange(sub.id, me, AuditAction::REMARK, AuditField::RAW_MARK, formatMark(before.rawMark), formatMark(raw), reason);
    if (feedbackChanged)
        audit_.recordChange(sub.id, me, AuditAction::REMARK, AuditField::FEEDBACK, before.feedback, feedback, reason);
    if (finalChanged)
        audit_.recordChange(sub.id, me, AuditAction::REMARK, AuditField::FINAL_MARK, formatMark(before.finalMark), formatMark(o.finalMark), reason);
    tx->commit();
    return MarkResult{raw, o.penalty, o.finalMark, o.days, SubmissionStatus::MARKED};
}

} // namespace lms
