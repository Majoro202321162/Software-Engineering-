// SQLite implementations of the repository interfaces. All SQL text lives in
// SqliteRepositories.cpp and uses bound parameters only (NFR-06).
#pragma once
#include "data/Database.h"
#include "data/Repositories.h"

namespace lms {

#define LMS_SQLITE_REPO(Name, Interface)            \
    class Name : public Interface {                 \
    public:                                         \
        explicit Name(Database& db) : db_(db) {}    \
    private:                                        \
        Database& db_;                              \
    public:

LMS_SQLITE_REPO(SqliteUserRepository, IUserRepository)
    std::optional<UserRecord> findById(int id) override;
    std::optional<UserRecord> findByUsername(const std::string& name) override;
    std::optional<UserRecord> findByStudentNumber(const std::string& number) override;
    std::optional<UserRecord> findActiveHod(int departmentId) override;
    int insert(const UserRecord& rec) override;
    void update(const UserRecord& rec) override;
    std::vector<UserRecord> listAll() override;
};

LMS_SQLITE_REPO(SqliteDepartmentRepository, IDepartmentRepository)
    std::optional<Department> findById(int id) override;
    std::optional<Department> findByName(const std::string& name) override;
    int insert(const std::string& name) override;
    void rename(int id, const std::string& name) override;
    std::vector<Department> listAll() override;
};

LMS_SQLITE_REPO(SqliteCourseRepository, ICourseRepository)
    std::optional<Course> findById(int id) override;
    std::optional<Course> findByCode(const std::string& code) override;
    int insert(const Course& c) override;
    void update(const Course& c) override;
    std::vector<Course> listAll() override;
    std::vector<Course> listByDepartment(int departmentId) override;
    std::vector<Course> listByLecturer(int userId) override;
    std::vector<Course> listByStudent(int studentId) override;
    void addLecturer(int courseId, int userId) override;
    void removeLecturer(int courseId, int userId) override;
    bool isLecturerOf(int courseId, int userId) override;
    std::vector<UserRecord> lecturersOf(int courseId) override;
    void setEnrolment(int courseId, int studentId, bool active, TimePoint now) override;
    bool isEnrolled(int courseId, int studentId) override;
    std::vector<UserRecord> studentsOf(int courseId) override;
};

LMS_SQLITE_REPO(SqliteAssignmentRepository, IAssignmentRepository)
    std::optional<Assignment> findById(int id) override;
    int insert(const Assignment& a) override;
    void update(const Assignment& a) override;
    std::vector<Assignment> listByCourse(int courseId) override;
};

LMS_SQLITE_REPO(SqliteExtensionRepository, IExtensionRepository)
    std::optional<Extension> latestFor(int assignmentId, int studentId) override;
    int insert(const Extension& e) override;
};

LMS_SQLITE_REPO(SqliteSubmissionRepository, ISubmissionRepository)
    std::optional<Submission> find(int assignmentId, int studentId) override;
    std::optional<Submission> findById(int id) override;
    int insert(const Submission& s) override;
    int addVersion(const SubmissionVersion& v) override;
    std::optional<SubmissionVersion> latestVersion(int submissionId) override;
    std::vector<SubmissionVersion> listVersions(int submissionId) override;
    void updateStatus(int submissionId, SubmissionStatus status) override;
    std::vector<SubmissionRow> listRows(const RowFilter& filter) override;
    std::vector<int> idsByStatus(int assignmentId, SubmissionStatus status) override;
    bool hasSubmissions(int assignmentId) override;
};

LMS_SQLITE_REPO(SqliteMarkRepository, IMarkRepository)
    std::optional<Mark> find(int submissionId) override;
    void save(const Mark& m) override;
    int releasedSince(int studentId, std::optional<TimePoint> since) override;
};

LMS_SQLITE_REPO(SqliteAuditRepository, IAuditRepository)
    void append(const AuditRecord& rec) override;
    std::vector<AuditRow> find(const AuditFilter& filter) override;
};

LMS_SQLITE_REPO(SqliteEventLogRepository, IEventLogRepository)
    void append(std::optional<int> userId, EventType type, const std::string& detail,
                TimePoint at) override;
};

// Transaction wrapper and backup hook over Database.
class SqliteUnitOfWork : public IUnitOfWork, public IDatabaseBackup {
public:
    explicit SqliteUnitOfWork(Database& db) : db_(db) {}
    std::unique_ptr<ITransaction> begin() override;
    void backupDatabase(const std::string& targetFile) override { db_.backupTo(targetFile); }
private:
    Database& db_;
};

#undef LMS_SQLITE_REPO

} // namespace lms
