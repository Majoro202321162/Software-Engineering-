#include "domain/Types.h"
#include "common/Exceptions.h"

namespace lms {

std::string toString(Role r) {
    switch (r) {
        case Role::ADMINISTRATOR: return "ADMINISTRATOR";
        case Role::LECTURER: return "LECTURER";
        case Role::HEAD_OF_DEPARTMENT: return "HEAD_OF_DEPARTMENT";
        case Role::STUDENT: return "STUDENT";
    }
    return "?";
}

Role roleFromString(const std::string& s) {
    if (s == "ADMINISTRATOR") return Role::ADMINISTRATOR;
    if (s == "LECTURER") return Role::LECTURER;
    if (s == "HEAD_OF_DEPARTMENT") return Role::HEAD_OF_DEPARTMENT;
    if (s == "STUDENT") return Role::STUDENT;
    throw ValidationException("Unknown role: " + s);
}

std::string roleLabel(Role r) {
    switch (r) {
        case Role::ADMINISTRATOR: return "Administrator";
        case Role::LECTURER: return "Lecturer";
        case Role::HEAD_OF_DEPARTMENT: return "Head of Department";
        case Role::STUDENT: return "Student";
    }
    return "?";
}

std::string toString(SubmissionStatus s) {
    switch (s) {
        case SubmissionStatus::SUBMITTED: return "SUBMITTED";
        case SubmissionStatus::LATE: return "LATE";
        case SubmissionStatus::MARKED: return "MARKED";
        case SubmissionStatus::RETURNED: return "RETURNED";
        case SubmissionStatus::MODERATED: return "MODERATED";
        case SubmissionStatus::RELEASED: return "RELEASED";
    }
    return "?";
}

SubmissionStatus statusFromString(const std::string& s) {
    if (s == "SUBMITTED") return SubmissionStatus::SUBMITTED;
    if (s == "LATE") return SubmissionStatus::LATE;
    if (s == "MARKED") return SubmissionStatus::MARKED;
    if (s == "RETURNED") return SubmissionStatus::RETURNED;
    if (s == "MODERATED") return SubmissionStatus::MODERATED;
    if (s == "RELEASED") return SubmissionStatus::RELEASED;
    throw ValidationException("Unknown status: " + s);
}

std::string toString(AuditAction a) {
    switch (a) {
        case AuditAction::INITIAL_MARKING: return "INITIAL_MARKING";
        case AuditAction::REMARK: return "REMARK";
        case AuditAction::MODERATION_ADJUSTMENT: return "MODERATION_ADJUSTMENT";
        case AuditAction::POST_RELEASE_CORRECTION: return "POST_RELEASE_CORRECTION";
    }
    return "?";
}

AuditAction auditActionFromString(const std::string& s) {
    if (s == "INITIAL_MARKING") return AuditAction::INITIAL_MARKING;
    if (s == "REMARK") return AuditAction::REMARK;
    if (s == "MODERATION_ADJUSTMENT") return AuditAction::MODERATION_ADJUSTMENT;
    if (s == "POST_RELEASE_CORRECTION") return AuditAction::POST_RELEASE_CORRECTION;
    throw ValidationException("Unknown audit action: " + s);
}

std::string toString(AuditField f) {
    switch (f) {
        case AuditField::RAW_MARK: return "RAW_MARK";
        case AuditField::FEEDBACK: return "FEEDBACK";
        case AuditField::FINAL_MARK: return "FINAL_MARK";
    }
    return "?";
}

AuditField auditFieldFromString(const std::string& s) {
    if (s == "RAW_MARK") return AuditField::RAW_MARK;
    if (s == "FEEDBACK") return AuditField::FEEDBACK;
    if (s == "FINAL_MARK") return AuditField::FINAL_MARK;
    throw ValidationException("Unknown audit field: " + s);
}

std::string toString(EventType e) {
    switch (e) {
        case EventType::ACCESS_DENIED: return "ACCESS_DENIED";
        case EventType::LOGIN_OK: return "LOGIN_OK";
        case EventType::LOGIN_FAILED: return "LOGIN_FAILED";
        case EventType::LOGIN_LOCKED: return "LOGIN_LOCKED";
        case EventType::FILE_ERROR: return "FILE_ERROR";
        case EventType::DB_ERROR: return "DB_ERROR";
    }
    return "?";
}

} // namespace lms
