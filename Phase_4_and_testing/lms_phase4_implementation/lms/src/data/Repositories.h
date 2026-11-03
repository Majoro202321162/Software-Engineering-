// Repository interfaces (Phase 3, Figure 3.5). The business layer depends only on these.
// IAuditRepository has no update or delete operation: audit records are append-only (FR-29).
#pragma once
#include "domain/Types.h"

#include <memory>
#include <optional>
#include <vector>

namespace lms {

class IUserRepository {
public:
    virtual ~IUserRepository() = default;
    virtual std::optional<UserRecord> findById(int id) = 0;
    virtual std::optional<UserRecord> findByUsername(const std::string& name) = 0;
    virtual std::optional<UserRecord> findByStudentNumber(const std::string& number) = 0;
    virtual std::optional<UserRecord> findActiveHod(int departmentId) = 0;
    virtual int insert(const UserRecord& rec) = 0;
    virtual void update(const UserRecord& rec) = 0;
    virtual std::vector<UserRecord> listAll() = 0;
};

class IDepartmentRepository {
public:
    virtual ~IDepartmentRepository() = default;
    virtual std::optional<Department> findById(int id) = 0;
    virtual std::optional<Department> findByName(const std::string& name) = 0;
    virtual int insert(const std::string& name) = 0;
    virtual void rename(int id, const std::string& name) = 0;
    virtual std::vector<Department> listAll() = 0;
};

class ICourseRepository {
public:
    virtual ~ICourseRepository() = default;
    virtual std::optional<Course> findById(int id) = 0;
    virtual std::optional<Course> findByCode(const std::string& code) = 0;
    virtual int insert(const Course& c) = 0;
    virtual void update(const Course& c) = 0;
    virtual std::vector<Course> listAll() = 0;
    virtual std::vector<Course> listByDepartment(int departmentId) = 0;
    virtual std::vector<Course> listByLecturer(int userId) = 0;
    virtual std::vector<Course> listByStudent(int studentId) = 0;      // active enrolments
    virtual void addLecturer(int courseId, int userId) = 0;
    virtual void removeLecturer(int courseId, int userId) = 0;
    virtual bool isLecturerOf(int courseId, int userId) = 0;
    virtual std::vector<UserRecord> lecturersOf(int courseId) = 0;
    virtual void setEnrolment(int courseId, int studentId, bool active, TimePoint now) = 0;
    virtual bool isEnrolled(int courseId, int studentId) = 0;          // active enrolment
    virtual std::vector<UserRecord> studentsOf(int courseId) = 0;      // active enrolments
};

class IAssignmentRepository {
public:
    virtual ~IAssignmentRepository() = default;
    virtual std::optional<Assignment> findById(int id) = 0;
    virtual int insert(const Assignment& a) = 0;
    virtual void update(const Assignment& a) = 0;
    virtual std::vector<Assignment> listByCourse(int courseId) = 0;
};

class IExtensionRepository {
public:
    virtual ~IExtensionRepository() = default;
    virtual std::optional<Extension> latestFor(int assignmentId, int studentId) = 0;
    virtual int insert(const Extension& e) = 0;
};

class ISubmissionRepository {
public:
    virtual ~ISubmissionRepository() = default;
    virtual std::optional<Submission> find(int assignmentId, int studentId) = 0;
    virtual std::optional<Submission> findById(int id) = 0;
    virtual int insert(const Submission& s) = 0;
    virtual int addVersion(const SubmissionVersion& v) = 0;
    virtual std::optional<SubmissionVersion> latestVersion(int submissionId) = 0;
    virtual std::vector<SubmissionVersion> listVersions(int submissionId) = 0;
    virtual void updateStatus(int submissionId, SubmissionStatus status) = 0;
    virtual std::vector<SubmissionRow> listRows(const RowFilter& filter) = 0;
    virtual std::vector<int> idsByStatus(int assignmentId, SubmissionStatus status) = 0;
    virtual bool hasSubmissions(int assignmentId) = 0;
};

class IMarkRepository {
public:
    virtual ~IMarkRepository() = default;
    virtual std::optional<Mark> find(int submissionId) = 0;
    virtual void save(const Mark& m) = 0;                               // insert or update
    virtual int releasedSince(int studentId, std::optional<TimePoint> since) = 0;
};

class IAuditRepository {
public:
    virtual ~IAuditRepository() = default;
    virtual void append(const AuditRecord& rec) = 0;
    virtual std::vector<AuditRow> find(const AuditFilter& filter) = 0;
};

class IEventLogRepository {
public:
    virtual ~IEventLogRepository() = default;
    virtual void append(std::optional<int> userId, EventType type, const std::string& detail,
                        TimePoint at) = 0;
};

// Unit of work: one transaction around a business operation (DD-07). The transaction
// rolls back in its destructor unless commit() was called, so an exception can never
// leave a half-saved mark. Nested use joins the outer transaction.
class ITransaction {
public:
    virtual ~ITransaction() = default;
    virtual void commit() = 0;
};

class IUnitOfWork {
public:
    virtual ~IUnitOfWork() = default;
    virtual std::unique_ptr<ITransaction> begin() = 0;
};

class IDatabaseBackup {
public:
    virtual ~IDatabaseBackup() = default;
    virtual void backupDatabase(const std::string& targetFile) = 0;
};

} // namespace lms
