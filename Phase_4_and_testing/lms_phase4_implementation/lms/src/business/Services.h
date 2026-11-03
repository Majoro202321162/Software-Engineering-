// Business logic layer (Phase 3, Figure 3.4, Table 3.1). Services apply BR-01 to BR-15.
// Every public method first calls PermissionPolicy::require() (Table 2.4).
// The parameter `s` is the logged-in Session.
#pragma once
#include "business/Lifecycle.h"
#include "business/Penalty.h"
#include "business/PermissionPolicy.h"
#include "business/Session.h"
#include "business/Validation.h"
#include "data/Repositories.h"
#include "infra/Infrastructure.h"

#include <memory>
#include <string>
#include <vector>

namespace lms {

// Bundle of repository interfaces handed to the services by main() (dependency injection).
struct RepoSet {
    IUserRepository& users;
    IDepartmentRepository& departments;
    ICourseRepository& courses;
    IAssignmentRepository& assignments;
    IExtensionRepository& extensions;
    ISubmissionRepository& submissions;
    IMarkRepository& marks;
    IAuditRepository& audit;
    IEventLogRepository& events;
    IUnitOfWork& uow;
};

// ---- data passed in and out of the services ------------------------------
struct NewUser {
    std::string username, fullName;
    Role role = Role::STUDENT;
    std::optional<int> departmentId;
    std::optional<std::string> studentNumber;
};
struct CreatedUser { int id = 0; std::string temporaryPassword; };

struct NewAssignment {
    int courseId = 0;
    std::string title, description;
    TimePoint deadline;
    int maxMark = 0;
};
struct AssignmentEdit {
    std::optional<std::string> title, description;
    std::optional<TimePoint> deadline;
    std::optional<int> maxMark;
    std::optional<bool> close;
};

struct SubmissionPreview {
    bool isResubmission = false, late = false, zeroMark = false;
    int daysLate = 0, maxMark = 0, nextVersionNo = 1;
    double penaltyMarks = 0;
    TimePoint now, effectiveDeadline;
    std::string fileName;
    long long fileSizeBytes = 0;
};
struct Receipt {
    int submissionId = 0, versionNo = 0;
    std::string fileName;
    long long sizeBytes = 0;
    TimePoint time;
    SubmissionStatus status = SubmissionStatus::SUBMITTED;
};
struct StudentAssignmentView {
    Course course;
    Assignment assignment;
    TimePoint effectiveDeadline;
    std::string statusText;               // "Not submitted", "Submitted", "Late", ...
};
struct StudentSubmissionView {
    std::string courseCode, assignmentTitle, statusText, fileName;
    int versionNo = 0, maxMark = 0;
    TimePoint uploadedAt;
    bool released = false;                // marks below are filled only when true (BR-02)
    double rawMark = 0, penalty = 0, finalMark = 0;
    int daysLate = 0;
    std::string feedback;
};
struct OpenedFile {
    std::string path, originalName, preview;
    long long sizeBytes = 0;
    int versionNo = 0;
};
struct MarkResult {
    double rawMark = 0, penalty = 0, finalMark = 0;
    int daysLate = 0;
    SubmissionStatus status = SubmissionStatus::MARKED;
};

// ---- scope checks shared by several services -----------------------------
class AccessScope {
public:
    AccessScope(const RepoSet& r, const PermissionPolicy& p) : r_(r), p_(p) {}
    Course lecturerCourse(const Session& s, int courseId) const;     // lecturer teaches it
    Course departmentCourse(const Session& s, int courseId) const;   // HoD's department owns it
    Course staffCourse(const Session& s, int courseId) const;        // either of the two
    Assignment assignment(int id) const;                             // throws if missing
    Submission submission(int id) const;
    int hodDepartment(const Session& s) const;
private:
    const RepoSet& r_;
    const PermissionPolicy& p_;
};

class AuditService {
public:
    AuditService(const RepoSet& r, const PermissionPolicy& p, IClock& clock)
        : r_(r), p_(p), clock_(clock) {}
    // Called inside the caller's transaction so a mark and its audit record commit together.
    void record(const AuditRecord& rec);
    void recordChange(int submissionId, int changedBy, AuditAction action, AuditField field,
                      const std::optional<std::string>& oldValue, const std::string& newValue,
                      const std::string& reason);
    std::vector<AuditRow> query(const Session& s, AuditFilter filter);   // HoD, own department
private:
    const RepoSet& r_;
    const PermissionPolicy& p_;
    IClock& clock_;
};

class AuthService {
public:
    AuthService(const RepoSet& r, IPasswordHasher& hasher, IClock& clock);
    std::unique_ptr<Session> login(const std::string& username, const std::string& password);
    void logout(Session& s);                       // the caller then discards the Session
    void changePassword(Session& s, const std::string& current, const std::string& newPw);
    static void validateNewPassword(const std::string& pw);
    static constexpr int kMaxFailedAttempts = 5;
    static constexpr int kLockMinutes = 5;
private:
    const RepoSet& r_;
    IPasswordHasher& hasher_;
    IClock& clock_;
    std::string dummyHash_;                        // checked for unknown usernames (CF7)
};

class UserService {
public:
    UserService(const RepoSet& r, IPasswordHasher& hasher, const PermissionPolicy& p, IClock& clock)
        : r_(r), hasher_(hasher), p_(p), clock_(clock) {}
    CreatedUser createUser(const Session& s, const NewUser& data);
    void editUser(const Session& s, int userId, const std::string& fullName,
                  std::optional<int> departmentId);
    void setActive(const Session& s, int userId, bool active);   // never deletes (BR-13)
    std::string resetPassword(const Session& s, int userId);
    std::vector<UserRecord> listUsers(const Session& s);
    // First start only: creates the first administrator when no account exists.
    CreatedUser createInitialAdministrator();
private:
    void checkDepartmentRole(const UserRecord& rec, std::optional<int> dept) const;
    const RepoSet& r_;
    IPasswordHasher& hasher_;
    const PermissionPolicy& p_;
    IClock& clock_;
};

class CourseService {
public:
    CourseService(const RepoSet& r, const PermissionPolicy& p, IClock& clock)
        : r_(r), p_(p), clock_(clock) {}
    int createDepartment(const Session& s, const std::string& name);
    void renameDepartment(const Session& s, int id, const std::string& name);
    std::vector<Department> listDepartments(const Session& s);
    int createCourse(const Session& s, const std::string& code, const std::string& title,
                     int departmentId);
    void editCourse(const Session& s, int courseId, const std::string& title, bool isActive);
    std::vector<Course> listAllCourses(const Session& s);            // administrator
    std::vector<Course> coursesFor(const Session& s);                // own courses by role
    void assignLecturer(const Session& s, int courseId, int userId);
    void removeLecturer(const Session& s, int courseId, int userId);
    void enrol(const Session& s, int courseId, int studentId);
    void unenrol(const Session& s, int courseId, int studentId);     // row kept (FR-10)
    std::vector<UserRecord> lecturersOf(const Session& s, int courseId);
    std::vector<UserRecord> studentsOf(const Session& s, int courseId);
private:
    const RepoSet& r_;
    const PermissionPolicy& p_;
    IClock& clock_;
};

class AssignmentService {
public:
    AssignmentService(const RepoSet& r, const PermissionPolicy& p, IClock& clock)
        : r_(r), p_(p), clock_(clock), scope_(r, p) {}
    int createAssignment(const Session& s, const NewAssignment& data);
    void editAssignment(const Session& s, int assignmentId, const AssignmentEdit& edit);
    // later of the assignment deadline and the student's latest extension (BR-03)
    TimePoint effectiveDeadline(int assignmentId, int studentId);
    void grantExtension(const Session& s, int assignmentId, int studentId, TimePoint newDeadline,
                        const std::string& reason);
    std::vector<Assignment> listByCourse(const Session& s, int courseId);
    // Students enrolled in the assignment's course (to choose who gets an extension).
    std::vector<UserRecord> enrolledStudents(const Session& s, int assignmentId);
private:
    const RepoSet& r_;
    const PermissionPolicy& p_;
    IClock& clock_;
    AccessScope scope_;
};

class SubmissionService {
public:
    SubmissionService(const RepoSet& r, const PermissionPolicy& p, IClock& clock, IFileStore& files,
                      const IPenaltyStrategy& penalty, AssignmentService& assignments)
        : r_(r), p_(p), clock_(clock), files_(files), penalty_(penalty),
          assignments_(assignments), scope_(r, p) {}
    std::vector<StudentAssignmentView> listAssignmentsForStudent(const Session& s);   // FR-14
    SubmissionPreview preview(const Session& s, int assignmentId, const std::string& path);
    // lateConfirmed must be true when the preview said the work is late (FR-18).
    Receipt submit(const Session& s, int assignmentId, const std::string& path, bool lateConfirmed);
    std::vector<StudentSubmissionView> listForStudent(const Session& s);              // FR-27
    std::vector<SubmissionRow> listForLecturer(const Session& s, int assignmentId);   // FR-20
    OpenedFile openFile(const Session& s, int submissionId);
    int releasedSinceLastLogin(const Session& s);                                     // FR-32
private:
    struct Check {
        Assignment assignment;
        Course course;
        TimePoint now, effective;
        std::optional<Submission> existing;
        int nextVersion = 1;
        bool late = false;
        int days = 0;
    };
    Check check(const Session& s, int assignmentId, const std::string& path);
    const RepoSet& r_;
    const PermissionPolicy& p_;
    IClock& clock_;
    IFileStore& files_;
    const IPenaltyStrategy& penalty_;
    AssignmentService& assignments_;
    AccessScope scope_;
};

class MarkingService {
public:
    MarkingService(const RepoSet& r, const PermissionPolicy& p, IClock& clock,
                   const IPenaltyStrategy& penalty, AssignmentService& assignments,
                   AuditService& audit)
        : r_(r), p_(p), clock_(clock), penalty_(penalty), assignments_(assignments),
          audit_(audit), scope_(r, p) {}
    MarkResult mark(const Session& s, int submissionId, double rawMark, const std::string& feedback);
    MarkResult remark(const Session& s, int submissionId, double rawMark,
                      const std::string& feedback, const std::string& reason);
private:
    struct Outcome { int days; double penalty, finalMark; };
    Outcome assess(const Assignment& a, const Submission& sub, double raw);
    const RepoSet& r_;
    const PermissionPolicy& p_;
    IClock& clock_;
    const IPenaltyStrategy& penalty_;
    AssignmentService& assignments_;
    AuditService& audit_;
    AccessScope scope_;
};

class ModerationService {
public:
    ModerationService(const RepoSet& r, const PermissionPolicy& p, IClock& clock,
                      const IPenaltyStrategy& penalty, AuditService& audit)
        : r_(r), p_(p), clock_(clock), penalty_(penalty), audit_(audit), scope_(r, p) {}
    std::vector<SubmissionRow> listForDepartment(const Session& s, RowFilter filter);   // FR-23
    int countAwaitingModeration(const Session& s, int assignmentId);                    // MARKED
    int countReleasable(const Session& s, int assignmentId);                            // MODERATED
    void approve(const Session& s, int submissionId);
    int approveAll(const Session& s, int assignmentId);                                 // FR-25
    void adjust(const Session& s, int submissionId, double finalMark, const std::string& reason);
    void returnForRemarking(const Session& s, int submissionId, const std::string& comment);
    void release(const Session& s, int submissionId);
    int releaseAll(const Session& s, int assignmentId);
    void correctReleased(const Session& s, int submissionId, double finalMark,
                         const std::string& reason);                                    // FR-35
private:
    struct Ctx { Submission sub; Assignment assignment; Mark mark; };
    Ctx load(const Session& s, int submissionId);
    void doApprove(const Session& s, Ctx& c);
    void doRelease(const Session& s, Ctx& c);
    const RepoSet& r_;
    const PermissionPolicy& p_;
    IClock& clock_;
    const IPenaltyStrategy& penalty_;
    AuditService& audit_;
    AccessScope scope_;
};

// Stand-alone helpers that nothing else depends on (can be cut first, risk R2).
class BackupService {
public:
    BackupService(IDatabaseBackup& db, IFileStore& files, const PermissionPolicy& p, IClock& clock)
        : db_(db), files_(files), p_(p), clock_(clock) {}
    std::string backup(const Session& s, const std::string& folder);    // FR-34; returns new folder
private:
    IDatabaseBackup& db_;
    IFileStore& files_;
    const PermissionPolicy& p_;
    IClock& clock_;
};

class ReportExporter {
public:
    ReportExporter(const RepoSet& r, const PermissionPolicy& p)
        : r_(r), p_(p), scope_(r, p) {}
    int exportReleasedMarks(const Session& s, int assignmentId, const std::string& csvPath); // FR-33
private:
    const RepoSet& r_;
    const PermissionPolicy& p_;
    AccessScope scope_;
};

} // namespace lms
