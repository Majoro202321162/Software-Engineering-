#include "business/Services.h"

namespace lms {

namespace {
void fillDaysLate(std::vector<SubmissionRow>& rows, const IPenaltyStrategy& penalty) {
    for (auto& r : rows)
        r.daysLate = r.mark ? r.mark->daysLate : penalty.daysLate(r.uploadedAt, r.effectiveDeadline);
}
} // namespace

// FR-14: courses, assignments, effective deadline and the student's own status.
std::vector<StudentAssignmentView> SubmissionService::listAssignmentsForStudent(const Session& s) {
    p_.require(s.getUser(), Function::VIEW_COURSES);
    std::vector<StudentAssignmentView> out;
    for (const Course& c : r_.courses.listByStudent(s.getUser().getId())) {
        for (const Assignment& a : r_.assignments.listByCourse(c.id)) {
            StudentAssignmentView v;
            v.course = c;
            v.assignment = a;
            v.effectiveDeadline = assignments_.effectiveDeadline(a.id, s.getUser().getId());
            auto sub = r_.submissions.find(a.id, s.getUser().getId());
            v.statusText = sub ? studentStatusText(sub->status) : (a.isClosed ? "Closed" : "Not submitted");
            out.push_back(std::move(v));
        }
    }
    return out;
}

// Shared by preview() and submit(): every rule that can refuse a submission.
SubmissionService::Check SubmissionService::check(const Session& s, int assignmentId,
                                                  const std::string& path) {
    p_.require(s.getUser(), Function::SUBMIT_WORK);
    Check c;
    c.assignment = scope_.assignment(assignmentId);
    auto course = r_.courses.findById(c.assignment.courseId);
    if (!course) throw ValidationException("That course does not exist.");
    c.course = *course;
    const int studentId = s.getUser().getId();
    if (!r_.courses.isEnrolled(c.course.id, studentId)) p_.deny(s.getUser(), "Submission to a course not enrolled in");
    if (!c.course.isActive) throw BusinessRuleException("That course is closed.");
    if (c.assignment.isClosed) throw BusinessRuleException("This assignment is closed and accepts no more submissions.");

    c.now = clock_.now();
    c.effective = assignments_.effectiveDeadline(assignmentId, studentId);
    files_.validate(path);                                     // FileException: existing work unchanged (FR-16)

    c.existing = r_.submissions.find(assignmentId, studentId);
    if (c.existing) {
        // BR-08: resubmission only while now <= effective deadline and the status is Submitted.
        if (c.existing->status != SubmissionStatus::SUBMITTED)
            throw BusinessRuleException(c.existing->status == SubmissionStatus::LATE
                ? "Late work cannot be resubmitted."
                : "This work has already been marked and cannot be replaced.");
        if (c.now > c.effective)
            throw BusinessRuleException("The deadline has passed, so the work can no longer be replaced.");
        auto latest = r_.submissions.latestVersion(c.existing->id);
        c.nextVersion = (latest ? latest->versionNo : 0) + 1;
    } else {
        c.late = c.now > c.effective;
        c.days = penalty_.daysLate(c.now, c.effective);
    }
    return c;
}

SubmissionPreview SubmissionService::preview(const Session& s, int assignmentId, const std::string& path) {
    Check c = check(s, assignmentId, path);
    SubmissionPreview p;
    p.isResubmission = c.existing.has_value();
    p.late = c.late;
    p.daysLate = c.days;
    p.zeroMark = c.late && penalty_.givesZero(c.days);
    p.penaltyMarks = c.late ? penalty_.penalty(c.assignment.maxMark, c.days) : 0.0;
    p.maxMark = c.assignment.maxMark;
    p.nextVersionNo = c.nextVersion;
    p.now = c.now;
    p.effectiveDeadline = c.effective;
    return p;
}

Receipt SubmissionService::submit(const Session& s, int assignmentId, const std::string& path,
                                  bool lateConfirmed) {
    Check c = check(s, assignmentId, path);
    if (c.late && !lateConfirmed)
        throw BusinessRuleException("This work is late. Please confirm the late warning first.");
    const int studentId = s.getUser().getId();
    const SubmissionStatus status = c.late ? SubmissionStatus::LATE : SubmissionStatus::SUBMITTED;
    if (c.existing) SubmissionLifecycle::require(c.existing->status, SubmissionStatus::SUBMITTED);   // T3

    StoredFile stored = files_.store(path, assignmentId, studentId, c.nextVersion);
    try {
        auto tx = r_.uow.begin();
        int submissionId;
        if (c.existing) {
            submissionId = c.existing->id;
        } else {                                                                      // T1 / T2
            Submission sub;
            sub.assignmentId = assignmentId;
            sub.studentId = studentId;
            sub.status = status;
            submissionId = r_.submissions.insert(sub);
        }
        SubmissionVersion v;
        v.submissionId = submissionId;
        v.versionNo = c.nextVersion;
        v.fileName = stored.originalName;
        v.storedPath = stored.storedPath;
        v.fileSizeBytes = stored.sizeBytes;
        v.uploadedAt = c.now;
        r_.submissions.addVersion(v);
        tx->commit();

        Receipt rc;
        rc.submissionId = submissionId;
        rc.versionNo = c.nextVersion;
        rc.fileName = stored.originalName;
        rc.sizeBytes = stored.sizeBytes;
        rc.time = c.now;
        rc.status = status;
        return rc;
    } catch (...) {
        files_.remove(stored.storedPath);                   // nothing half-saved (FR-16, FR-31)
        throw;
    }
}

// FR-27, BR-02: marks and feedback only for RELEASED submissions.
std::vector<StudentSubmissionView> SubmissionService::listForStudent(const Session& s) {
    p_.require(s.getUser(), Function::VIEW_RELEASED_MARKS);
    RowFilter f;
    f.studentId = s.getUser().getId();
    std::vector<StudentSubmissionView> out;
    for (const SubmissionRow& r : r_.submissions.listRows(f)) {
        StudentSubmissionView v;
        v.courseCode = r.courseCode;
        v.assignmentTitle = r.assignmentTitle;
        v.statusText = studentStatusText(r.status);
        v.fileName = r.fileName;
        v.versionNo = r.latestVersionNo;
        v.maxMark = r.maxMark;
        v.uploadedAt = r.uploadedAt;
        if (r.status == SubmissionStatus::RELEASED && r.mark) {
            v.released = true;
            v.rawMark = r.mark->rawMark;
            v.penalty = r.mark->penalty;
            v.finalMark = r.mark->finalMark;
            v.daysLate = r.mark->daysLate;
            v.feedback = r.mark->feedback;
        }
        out.push_back(std::move(v));
    }
    return out;
}

std::vector<SubmissionRow> SubmissionService::listForLecturer(const Session& s, int assignmentId) {
    p_.require(s.getUser(), Function::VIEW_SUBMISSIONS);
    Assignment a = scope_.assignment(assignmentId);
    scope_.staffCourse(s, a.courseId);
    RowFilter f;
    f.assignmentId = assignmentId;
    auto rows = r_.submissions.listRows(f);
    fillDaysLate(rows, penalty_);
    return rows;
}

OpenedFile SubmissionService::openFile(const Session& s, int submissionId) {
    p_.require(s.getUser(), Function::VIEW_SUBMISSIONS);
    Submission sub = scope_.submission(submissionId);
    Assignment a = scope_.assignment(sub.assignmentId);
    scope_.staffCourse(s, a.courseId);
    auto v = r_.submissions.latestVersion(submissionId);
    if (!v) throw BusinessRuleException("That submission has no file.");
    OpenedFile out;
    out.path = files_.open(v->storedPath);
    out.originalName = v->fileName;
    out.sizeBytes = v->fileSizeBytes;
    out.versionNo = v->versionNo;
    out.preview = files_.readTextPreview(v->storedPath, 15);
    return out;
}

int SubmissionService::releasedSinceLastLogin(const Session& s) {
    p_.require(s.getUser(), Function::VIEW_RELEASED_MARKS);
    return r_.marks.releasedSince(s.getUser().getId(), s.previousLogin());
}

} // namespace lms
