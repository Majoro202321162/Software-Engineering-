// Enumerations and plain record types shared by all layers (Phase 3, Figure 3.1).
#pragma once
#include "common/TimeUtil.h"

#include <optional>
#include <set>
#include <string>
#include <vector>

namespace lms {

enum class Role { ADMINISTRATOR, LECTURER, HEAD_OF_DEPARTMENT, STUDENT };

// Every action in the permission matrix (SRS Table 4.4). CHANGE_PASSWORD is
// held by all roles; EXPORT_MARKS supports FR-33.
enum class Function {
    CHANGE_PASSWORD, MANAGE_ACCOUNTS, MANAGE_STRUCTURE, BACK_UP_DATA,
    MANAGE_ASSIGNMENTS, GRANT_EXTENSION, VIEW_SUBMISSIONS, MARK_SUBMISSION,
    MODERATE_MARKS, RELEASE_MARKS, CORRECT_MARK, VIEW_AUDIT_LOG, EXPORT_MARKS,
    VIEW_COURSES, SUBMIT_WORK, VIEW_RELEASED_MARKS
};

enum class SubmissionStatus { SUBMITTED, LATE, MARKED, RETURNED, MODERATED, RELEASED };

enum class AuditAction { INITIAL_MARKING, REMARK, MODERATION_ADJUSTMENT, POST_RELEASE_CORRECTION };
enum class AuditField { RAW_MARK, FEEDBACK, FINAL_MARK };
enum class EventType { ACCESS_DENIED, LOGIN_OK, LOGIN_FAILED, LOGIN_LOCKED, FILE_ERROR, DB_ERROR };

std::string toString(Role r);
Role roleFromString(const std::string& s);
std::string roleLabel(Role r);                    // readable text for menus
std::string toString(SubmissionStatus s);
SubmissionStatus statusFromString(const std::string& s);
std::string toString(AuditAction a);
AuditAction auditActionFromString(const std::string& s);
std::string toString(AuditField f);
AuditField auditFieldFromString(const std::string& s);
std::string toString(EventType e);

// ---- records (data holders; no database knowledge) ----------------------
struct UserRecord {
    int id = 0;
    std::string username, fullName, role, passwordHash;
    std::optional<int> departmentId;
    std::optional<std::string> studentNumber;
    bool mustChangePassword = true;
    bool isActive = true;
    int failedAttempts = 0;
    std::optional<TimePoint> lockedUntil;
    std::optional<TimePoint> lastLoginAt;
};

struct Department { int id = 0; std::string name; };

struct Course {
    int id = 0;
    std::string code, title;
    int departmentId = 0;
    bool isActive = true;
};

struct Assignment {
    int id = 0, courseId = 0;
    std::string title, description;
    TimePoint deadline;
    int maxMark = 0;
    bool isClosed = false;
    int createdBy = 0;
    TimePoint createdAt;
};

struct Extension {
    int id = 0, assignmentId = 0, studentId = 0;
    TimePoint newDeadline;
    std::string reason;
    int grantedBy = 0;
    TimePoint grantedAt;
};

struct Submission {
    int id = 0, assignmentId = 0, studentId = 0;
    SubmissionStatus status = SubmissionStatus::SUBMITTED;
};

struct SubmissionVersion {
    int id = 0, submissionId = 0, versionNo = 0;
    std::string fileName, storedPath;
    long long fileSizeBytes = 0;
    TimePoint uploadedAt;
};

struct Mark {
    int submissionId = 0;
    double rawMark = 0, penalty = 0, finalMark = 0;
    int daysLate = 0;
    std::string feedback;
    int markedBy = 0;
    TimePoint markedAt;
    std::optional<std::string> hodComment;
    std::optional<int> moderatedBy;
    std::optional<TimePoint> moderatedAt;
    std::optional<int> releasedBy;
    std::optional<TimePoint> releasedAt;
};

struct AuditRecord {
    int id = 0, submissionId = 0, changedBy = 0;
    TimePoint changedAt;
    AuditAction actionType = AuditAction::INITIAL_MARKING;
    AuditField fieldName = AuditField::RAW_MARK;
    std::optional<std::string> oldValue;
    std::string newValue, reason;
};

// Audit record joined with names for display (FR-30).
struct AuditRow {
    AuditRecord record;
    std::string changedByName, studentName, studentNumber, courseCode, assignmentTitle;
};

struct AuditFilter {
    std::optional<int> departmentId, assignmentId, studentId;
    std::optional<TimePoint> from, to;
};

// A submission joined with the data a list screen needs (FR-14, FR-20, FR-23).
struct SubmissionRow {
    int submissionId = 0, assignmentId = 0, courseId = 0, studentId = 0;
    std::string assignmentTitle, courseCode, studentName, studentNumber;
    int maxMark = 0;
    SubmissionStatus status = SubmissionStatus::SUBMITTED;
    int latestVersionNo = 0;
    std::string fileName;
    TimePoint uploadedAt, effectiveDeadline;
    std::optional<Mark> mark;
    std::string markedByName;
    int daysLate = 0;                 // stored snapshot if marked, else calculated
};

struct RowFilter {
    std::optional<int> assignmentId, courseId, departmentId, studentId, lecturerId;
    std::optional<SubmissionStatus> status;
};

} // namespace lms
