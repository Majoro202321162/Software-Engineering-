#include "business/Services.h"

namespace lms {

// Loads the submission, its mark and checks scope (BR-10): the department must own the
// course and the HoD must not be the person who entered the mark.
ModerationService::Ctx ModerationService::load(const Session& s, int submissionId) {
    Ctx c;
    c.sub = scope_.submission(submissionId);
    c.assignment = scope_.assignment(c.sub.assignmentId);
    scope_.departmentCourse(s, c.assignment.courseId);
    auto m = r_.marks.find(submissionId);
    if (!m) throw BusinessRuleException("This submission has not been marked yet.");
    if (m->markedBy == s.getUser().getId())
        throw BusinessRuleException("You cannot moderate a mark that you entered yourself.");
    c.mark = *m;
    return c;
}

std::vector<SubmissionRow> ModerationService::listForDepartment(const Session& s, RowFilter filter) {
    p_.require(s.getUser(), Function::MODERATE_MARKS);
    filter.departmentId = scope_.hodDepartment(s);            // cannot be overridden by the caller
    auto rows = r_.submissions.listRows(filter);
    for (auto& r : rows)
        r.daysLate = r.mark ? r.mark->daysLate : penalty_.daysLate(r.uploadedAt, r.effectiveDeadline);
    return rows;
}

int ModerationService::countAwaitingModeration(const Session& s, int assignmentId) {
    p_.require(s.getUser(), Function::MODERATE_MARKS);
    scope_.departmentCourse(s, scope_.assignment(assignmentId).courseId);
    return static_cast<int>(r_.submissions.idsByStatus(assignmentId, SubmissionStatus::MARKED).size());
}

int ModerationService::countReleasable(const Session& s, int assignmentId) {
    p_.require(s.getUser(), Function::RELEASE_MARKS);
    scope_.departmentCourse(s, scope_.assignment(assignmentId).courseId);
    return static_cast<int>(r_.submissions.idsByStatus(assignmentId, SubmissionStatus::MODERATED).size());
}

void ModerationService::doApprove(const Session& s, Ctx& c) {                     // T8
    SubmissionLifecycle::require(c.sub.status, SubmissionStatus::MODERATED);
    c.mark.moderatedBy = s.getUser().getId();
    c.mark.moderatedAt = clock_.now();
    c.mark.hodComment.reset();
    r_.marks.save(c.mark);
    r_.submissions.updateStatus(c.sub.id, SubmissionStatus::MODERATED);
}

void ModerationService::approve(const Session& s, int submissionId) {
    p_.require(s.getUser(), Function::MODERATE_MARKS);
    Ctx c = load(s, submissionId);
    auto tx = r_.uow.begin();
    doApprove(s, c);
    tx->commit();
}

// FR-25: the caller shows countAwaitingModeration() and asks for confirmation first.
int ModerationService::approveAll(const Session& s, int assignmentId) {
    p_.require(s.getUser(), Function::MODERATE_MARKS);
    scope_.departmentCourse(s, scope_.assignment(assignmentId).courseId);
    auto ids = r_.submissions.idsByStatus(assignmentId, SubmissionStatus::MARKED);
    auto tx = r_.uow.begin();
    for (int id : ids) {
        Ctx c = load(s, id);
        doApprove(s, c);
    }
    tx->commit();
    return static_cast<int>(ids.size());
}

void ModerationService::adjust(const Session& s, int submissionId, double finalMark, const std::string& rawReason) {
    p_.require(s.getUser(), Function::MODERATE_MARKS);
    Ctx c = load(s, submissionId);
    const std::string reason = trim(rawReason);
    requireMarkValue("The final mark", finalMark, c.assignment.maxMark);
    requireLength("Reason", reason, 10, 500);
    if (std::fabs(finalMark - c.mark.finalMark) < 1e-9)
        throw ValidationException("The new final mark is the same as the current one. Use Approve instead.");
    SubmissionLifecycle::require(c.sub.status, SubmissionStatus::MODERATED);

    const double old = c.mark.finalMark;
    auto tx = r_.uow.begin();
    c.mark.finalMark = finalMark;                              // HoD adjustment replaces the final mark (BR-07)
    audit_.recordChange(c.sub.id, s.getUser().getId(), AuditAction::MODERATION_ADJUSTMENT,
                        AuditField::FINAL_MARK, formatMark(old), formatMark(finalMark), reason);
    doApprove(s, c);
    tx->commit();
}

void ModerationService::returnForRemarking(const Session& s, int submissionId, const std::string& rawComment) {
    p_.require(s.getUser(), Function::MODERATE_MARKS);
    Ctx c = load(s, submissionId);
    const std::string comment = trim(rawComment);
    requireLength("Comment", comment, 1, 500);
    SubmissionLifecycle::require(c.sub.status, SubmissionStatus::RETURNED);       // T9
    auto tx = r_.uow.begin();
    c.mark.hodComment = comment;
    r_.marks.save(c.mark);
    r_.submissions.updateStatus(c.sub.id, SubmissionStatus::RETURNED);
    tx->commit();
}

void ModerationService::doRelease(const Session& s, Ctx& c) {                     // T11
    SubmissionLifecycle::require(c.sub.status, SubmissionStatus::RELEASED);
    c.mark.releasedBy = s.getUser().getId();
    c.mark.releasedAt = clock_.now();
    r_.marks.save(c.mark);
    r_.submissions.updateStatus(c.sub.id, SubmissionStatus::RELEASED);
}

void ModerationService::release(const Session& s, int submissionId) {
    p_.require(s.getUser(), Function::RELEASE_MARKS);
    Ctx c = load(s, submissionId);
    auto tx = r_.uow.begin();
    doRelease(s, c);
    tx->commit();
}

int ModerationService::releaseAll(const Session& s, int assignmentId) {
    p_.require(s.getUser(), Function::RELEASE_MARKS);
    scope_.departmentCourse(s, scope_.assignment(assignmentId).courseId);
    auto ids = r_.submissions.idsByStatus(assignmentId, SubmissionStatus::MODERATED);
    auto tx = r_.uow.begin();
    for (int id : ids) {
        Ctx c = load(s, id);
        doRelease(s, c);
    }
    tx->commit();
    return static_cast<int>(ids.size());
}

// T12: status stays Released; the correction is recorded with who, when, old, new and why.
void ModerationService::correctReleased(const Session& s, int submissionId, double finalMark,
                                        const std::string& rawReason) {
    p_.require(s.getUser(), Function::CORRECT_MARK);
    Ctx c = load(s, submissionId);
    const std::string reason = trim(rawReason);
    requireMarkValue("The final mark", finalMark, c.assignment.maxMark);
    requireLength("Reason", reason, 10, 500);
    SubmissionLifecycle::require(c.sub.status, SubmissionStatus::RELEASED);
    if (std::fabs(finalMark - c.mark.finalMark) < 1e-9)
        throw ValidationException("The new final mark is the same as the current one.");

    const double old = c.mark.finalMark;
    auto tx = r_.uow.begin();
    c.mark.finalMark = finalMark;
    r_.marks.save(c.mark);
    audit_.recordChange(c.sub.id, s.getUser().getId(), AuditAction::POST_RELEASE_CORRECTION,
                        AuditField::FINAL_MARK, formatMark(old), formatMark(finalMark), reason);
    tx->commit();
}

} // namespace lms
