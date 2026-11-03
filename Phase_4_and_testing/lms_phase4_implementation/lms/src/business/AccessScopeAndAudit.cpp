#include "business/Services.h"

namespace lms {

// ===== AccessScope ==========================================================
Assignment AccessScope::assignment(int id) const {
    auto a = r_.assignments.findById(id);
    if (!a) throw ValidationException("That assignment does not exist.");
    return *a;
}

Submission AccessScope::submission(int id) const {
    auto s = r_.submissions.findById(id);
    if (!s) throw ValidationException("That submission does not exist.");
    return *s;
}

int AccessScope::hodDepartment(const Session& s) const {
    auto d = s.getUser().departmentId();
    if (s.getUser().getRole() != Role::HEAD_OF_DEPARTMENT || !d)
        p_.deny(s.getUser(), "No department for this account");
    return *d;
}

Course AccessScope::lecturerCourse(const Session& s, int courseId) const {
    auto c = r_.courses.findById(courseId);
    if (!c) throw ValidationException("That course does not exist.");
    if (s.getUser().getRole() != Role::LECTURER || !r_.courses.isLecturerOf(courseId, s.getUser().getId()))
        p_.deny(s.getUser(), "Course " + std::to_string(courseId) + " is not taught by this user");
    return *c;
}

Course AccessScope::departmentCourse(const Session& s, int courseId) const {
    auto c = r_.courses.findById(courseId);
    if (!c) throw ValidationException("That course does not exist.");
    if (c->departmentId != hodDepartment(s))
        p_.deny(s.getUser(), "Course " + std::to_string(courseId) + " is in another department");
    return *c;
}

Course AccessScope::staffCourse(const Session& s, int courseId) const {
    return s.getUser().getRole() == Role::HEAD_OF_DEPARTMENT ? departmentCourse(s, courseId)
                                                              : lecturerCourse(s, courseId);
}

// ===== AuditService =========================================================
void AuditService::record(const AuditRecord& rec) {
    r_.audit.append(rec);
}

void AuditService::recordChange(int submissionId, int changedBy, AuditAction action,
                                AuditField field, const std::optional<std::string>& oldValue,
                                const std::string& newValue, const std::string& reason) {
    AuditRecord rec;
    rec.submissionId = submissionId;
    rec.changedBy = changedBy;
    rec.changedAt = clock_.now();
    rec.actionType = action;
    rec.fieldName = field;
    rec.oldValue = oldValue;
    rec.newValue = newValue;
    rec.reason = reason;
    record(rec);
}

std::vector<AuditRow> AuditService::query(const Session& s, AuditFilter filter) {
    p_.require(s.getUser(), Function::VIEW_AUDIT_LOG);
    // A HoD sees only the audit records of the own department (Table 4.4).
    filter.departmentId = AccessScope(r_, p_).hodDepartment(s);
    return r_.audit.find(filter);
}

} // namespace lms
