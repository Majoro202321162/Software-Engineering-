#include "data/SqliteRepositories.h"
#include "common/Exceptions.h"

namespace lms {
namespace {

std::optional<TimePoint> optTime(const Statement& st, int col) {
    auto t = st.columnOptText(col);
    if (!t) return std::nullopt;
    return fromIso(*t);
}

std::optional<std::string> isoOpt(const std::optional<TimePoint>& t) {
    if (!t) return std::nullopt;
    return toIso(*t);
}

// ---- users ----
const char* kUserCols =
    "user_id, username, full_name, role, department_id, student_number, password_hash, "
    "must_change_password, is_active, failed_attempts, locked_until, last_login_at";

UserRecord readUser(const Statement& st) {
    UserRecord r;
    r.id = st.columnInt(0);
    r.username = st.columnText(1);
    r.fullName = st.columnText(2);
    r.role = st.columnText(3);
    r.departmentId = st.columnOptInt(4);
    r.studentNumber = st.columnOptText(5);
    r.passwordHash = st.columnText(6);
    r.mustChangePassword = st.columnInt(7) != 0;
    r.isActive = st.columnInt(8) != 0;
    r.failedAttempts = st.columnInt(9);
    r.lockedUntil = optTime(st, 10);
    r.lastLoginAt = optTime(st, 11);
    return r;
}

std::optional<UserRecord> oneUser(Database& db, const std::string& where, const std::string& value) {
    Statement st = db.prepare(std::string("SELECT ") + kUserCols + " FROM users WHERE " + where);
    st.bind(1, value);
    if (!st.step()) return std::nullopt;
    return readUser(st);
}

// ---- marks ----
const char* kMarkCols =
    "submission_id, raw_mark, days_late, penalty, final_mark, feedback, marked_by, marked_at, "
    "hod_comment, moderated_by, moderated_at, released_by, released_at";

Mark readMark(const Statement& st, int base = 0) {
    Mark m;
    m.submissionId = st.columnInt(base + 0);
    m.rawMark = st.columnDouble(base + 1);
    m.daysLate = st.columnInt(base + 2);
    m.penalty = st.columnDouble(base + 3);
    m.finalMark = st.columnDouble(base + 4);
    m.feedback = st.columnText(base + 5);
    m.markedBy = st.columnInt(base + 6);
    m.markedAt = fromIso(st.columnText(base + 7));
    m.hodComment = st.columnOptText(base + 8);
    m.moderatedBy = st.columnOptInt(base + 9);
    m.moderatedAt = optTime(st, base + 10);
    m.releasedBy = st.columnOptInt(base + 11);
    m.releasedAt = optTime(st, base + 12);
    return m;
}

Course readCourse(const Statement& st) {
    Course c;
    c.id = st.columnInt(0);
    c.code = st.columnText(1);
    c.title = st.columnText(2);
    c.departmentId = st.columnInt(3);
    c.isActive = st.columnInt(4) != 0;
    return c;
}
const char* kCourseCols = "c.course_id, c.code, c.title, c.department_id, c.is_active";

Assignment readAssignment(const Statement& st) {
    Assignment a;
    a.id = st.columnInt(0);
    a.courseId = st.columnInt(1);
    a.title = st.columnText(2);
    a.description = st.columnText(3);
    a.deadline = fromIso(st.columnText(4));
    a.maxMark = st.columnInt(5);
    a.isClosed = st.columnInt(6) != 0;
    a.createdBy = st.columnInt(7);
    a.createdAt = fromIso(st.columnText(8));
    return a;
}
const char* kAssignCols =
    "assignment_id, course_id, title, description, deadline, max_mark, is_closed, created_by, created_at";

} // namespace

// ===== SqliteUserRepository =================================================
std::optional<UserRecord> SqliteUserRepository::findById(int id) {
    Statement st = db_.prepare(std::string("SELECT ") + kUserCols + " FROM users WHERE user_id = ?");
    st.bind(1, id);
    if (!st.step()) return std::nullopt;
    return readUser(st);
}

std::optional<UserRecord> SqliteUserRepository::findByUsername(const std::string& name) {
    return oneUser(db_, "username = ?", name);                  // column is COLLATE NOCASE
}

std::optional<UserRecord> SqliteUserRepository::findByStudentNumber(const std::string& number) {
    return oneUser(db_, "student_number = ?", number);
}

std::optional<UserRecord> SqliteUserRepository::findActiveHod(int departmentId) {
    Statement st = db_.prepare(std::string("SELECT ") + kUserCols +
        " FROM users WHERE role = 'HEAD_OF_DEPARTMENT' AND is_active = 1 AND department_id = ?");
    st.bind(1, departmentId);
    if (!st.step()) return std::nullopt;
    return readUser(st);
}

int SqliteUserRepository::insert(const UserRecord& r) {
    Statement st = db_.prepare(
        "INSERT INTO users (username, full_name, role, department_id, student_number, "
        "password_hash, must_change_password, is_active, failed_attempts, locked_until, last_login_at) "
        "VALUES (?,?,?,?,?,?,?,?,?,?,?)");
    st.bind(1, r.username); st.bind(2, r.fullName); st.bind(3, r.role);
    st.bind(4, r.departmentId); st.bind(5, r.studentNumber); st.bind(6, r.passwordHash);
    st.bind(7, r.mustChangePassword); st.bind(8, r.isActive); st.bind(9, r.failedAttempts);
    st.bind(10, isoOpt(r.lockedUntil)); st.bind(11, isoOpt(r.lastLoginAt));
    st.step();
    return static_cast<int>(db_.lastInsertId());
}

void SqliteUserRepository::update(const UserRecord& r) {
    Statement st = db_.prepare(
        "UPDATE users SET username=?, full_name=?, role=?, department_id=?, student_number=?, "
        "password_hash=?, must_change_password=?, is_active=?, failed_attempts=?, locked_until=?, "
        "last_login_at=? WHERE user_id=?");
    st.bind(1, r.username); st.bind(2, r.fullName); st.bind(3, r.role);
    st.bind(4, r.departmentId); st.bind(5, r.studentNumber); st.bind(6, r.passwordHash);
    st.bind(7, r.mustChangePassword); st.bind(8, r.isActive); st.bind(9, r.failedAttempts);
    st.bind(10, isoOpt(r.lockedUntil)); st.bind(11, isoOpt(r.lastLoginAt)); st.bind(12, r.id);
    st.step();
}

std::vector<UserRecord> SqliteUserRepository::listAll() {
    Statement st = db_.prepare(std::string("SELECT ") + kUserCols + " FROM users ORDER BY role, username");
    std::vector<UserRecord> out;
    while (st.step()) out.push_back(readUser(st));
    return out;
}

// ===== SqliteDepartmentRepository ===========================================
std::optional<Department> SqliteDepartmentRepository::findById(int id) {
    Statement st = db_.prepare("SELECT department_id, name FROM departments WHERE department_id = ?");
    st.bind(1, id);
    if (!st.step()) return std::nullopt;
    return Department{st.columnInt(0), st.columnText(1)};
}

std::optional<Department> SqliteDepartmentRepository::findByName(const std::string& name) {
    Statement st = db_.prepare("SELECT department_id, name FROM departments WHERE name = ?");
    st.bind(1, name);
    if (!st.step()) return std::nullopt;
    return Department{st.columnInt(0), st.columnText(1)};
}

int SqliteDepartmentRepository::insert(const std::string& name) {
    Statement st = db_.prepare("INSERT INTO departments (name) VALUES (?)");
    st.bind(1, name);
    st.step();
    return static_cast<int>(db_.lastInsertId());
}

void SqliteDepartmentRepository::rename(int id, const std::string& name) {
    Statement st = db_.prepare("UPDATE departments SET name = ? WHERE department_id = ?");
    st.bind(1, name); st.bind(2, id);
    st.step();
}

std::vector<Department> SqliteDepartmentRepository::listAll() {
    Statement st = db_.prepare("SELECT department_id, name FROM departments ORDER BY name");
    std::vector<Department> out;
    while (st.step()) out.push_back(Department{st.columnInt(0), st.columnText(1)});
    return out;
}

// ===== SqliteCourseRepository ===============================================
std::optional<Course> SqliteCourseRepository::findById(int id) {
    Statement st = db_.prepare(std::string("SELECT ") + kCourseCols + " FROM courses c WHERE c.course_id = ?");
    st.bind(1, id);
    if (!st.step()) return std::nullopt;
    return readCourse(st);
}

std::optional<Course> SqliteCourseRepository::findByCode(const std::string& code) {
    Statement st = db_.prepare(std::string("SELECT ") + kCourseCols + " FROM courses c WHERE c.code = ?");
    st.bind(1, code);
    if (!st.step()) return std::nullopt;
    return readCourse(st);
}

int SqliteCourseRepository::insert(const Course& c) {
    Statement st = db_.prepare("INSERT INTO courses (code, title, department_id, is_active) VALUES (?,?,?,?)");
    st.bind(1, c.code); st.bind(2, c.title); st.bind(3, c.departmentId); st.bind(4, c.isActive);
    st.step();
    return static_cast<int>(db_.lastInsertId());
}

void SqliteCourseRepository::update(const Course& c) {
    Statement st = db_.prepare("UPDATE courses SET code=?, title=?, department_id=?, is_active=? WHERE course_id=?");
    st.bind(1, c.code); st.bind(2, c.title); st.bind(3, c.departmentId); st.bind(4, c.isActive);
    st.bind(5, c.id);
    st.step();
}

std::vector<Course> SqliteCourseRepository::listAll() {
    Statement st = db_.prepare(std::string("SELECT ") + kCourseCols + " FROM courses c ORDER BY c.code");
    std::vector<Course> out;
    while (st.step()) out.push_back(readCourse(st));
    return out;
}

std::vector<Course> SqliteCourseRepository::listByDepartment(int departmentId) {
    Statement st = db_.prepare(std::string("SELECT ") + kCourseCols +
                               " FROM courses c WHERE c.department_id = ? ORDER BY c.code");
    st.bind(1, departmentId);
    std::vector<Course> out;
    while (st.step()) out.push_back(readCourse(st));
    return out;
}

std::vector<Course> SqliteCourseRepository::listByLecturer(int userId) {
    Statement st = db_.prepare(std::string("SELECT ") + kCourseCols +
        " FROM courses c JOIN course_lecturers cl ON cl.course_id = c.course_id "
        "WHERE cl.lecturer_id = ? ORDER BY c.code");
    st.bind(1, userId);
    std::vector<Course> out;
    while (st.step()) out.push_back(readCourse(st));
    return out;
}

std::vector<Course> SqliteCourseRepository::listByStudent(int studentId) {
    Statement st = db_.prepare(std::string("SELECT ") + kCourseCols +
        " FROM courses c JOIN enrolments e ON e.course_id = c.course_id "
        "WHERE e.student_id = ? AND e.unenrolled_at IS NULL AND c.is_active = 1 ORDER BY c.code");
    st.bind(1, studentId);
    std::vector<Course> out;
    while (st.step()) out.push_back(readCourse(st));
    return out;
}

void SqliteCourseRepository::addLecturer(int courseId, int userId) {
    Statement st = db_.prepare("INSERT OR IGNORE INTO course_lecturers (course_id, lecturer_id) VALUES (?,?)");
    st.bind(1, courseId); st.bind(2, userId);
    st.step();
}

void SqliteCourseRepository::removeLecturer(int courseId, int userId) {
    Statement st = db_.prepare("DELETE FROM course_lecturers WHERE course_id = ? AND lecturer_id = ?");
    st.bind(1, courseId); st.bind(2, userId);
    st.step();
}

bool SqliteCourseRepository::isLecturerOf(int courseId, int userId) {
    Statement st = db_.prepare("SELECT 1 FROM course_lecturers WHERE course_id = ? AND lecturer_id = ?");
    st.bind(1, courseId); st.bind(2, userId);
    return st.step();
}

std::vector<UserRecord> SqliteCourseRepository::lecturersOf(int courseId) {
    Statement st = db_.prepare(
        "SELECT u.user_id, u.username, u.full_name, u.role, u.department_id, u.student_number, "
        "u.password_hash, u.must_change_password, u.is_active, u.failed_attempts, u.locked_until, "
        "u.last_login_at FROM users u JOIN course_lecturers cl ON cl.lecturer_id = u.user_id "
        "WHERE cl.course_id = ? ORDER BY u.full_name");
    st.bind(1, courseId);
    std::vector<UserRecord> out;
    while (st.step()) out.push_back(readUser(st));
    return out;
}

void SqliteCourseRepository::setEnrolment(int courseId, int studentId, bool active, TimePoint now) {
    if (active) {
        Statement st = db_.prepare(
            "INSERT INTO enrolments (course_id, student_id, enrolled_at, unenrolled_at) VALUES (?,?,?,NULL) "
            "ON CONFLICT(course_id, student_id) DO UPDATE SET enrolled_at = excluded.enrolled_at, "
            "unenrolled_at = NULL");
        st.bind(1, courseId); st.bind(2, studentId); st.bind(3, toIso(now));
        st.step();
    } else {   // the row is kept so submissions and marks stay valid (FR-10, BR-13)
        Statement st = db_.prepare(
            "UPDATE enrolments SET unenrolled_at = ? WHERE course_id = ? AND student_id = ? "
            "AND unenrolled_at IS NULL");
        st.bind(1, toIso(now)); st.bind(2, courseId); st.bind(3, studentId);
        st.step();
    }
}

bool SqliteCourseRepository::isEnrolled(int courseId, int studentId) {
    Statement st = db_.prepare(
        "SELECT 1 FROM enrolments WHERE course_id = ? AND student_id = ? AND unenrolled_at IS NULL");
    st.bind(1, courseId); st.bind(2, studentId);
    return st.step();
}

std::vector<UserRecord> SqliteCourseRepository::studentsOf(int courseId) {
    Statement st = db_.prepare(
        "SELECT u.user_id, u.username, u.full_name, u.role, u.department_id, u.student_number, "
        "u.password_hash, u.must_change_password, u.is_active, u.failed_attempts, u.locked_until, "
        "u.last_login_at FROM users u JOIN enrolments e ON e.student_id = u.user_id "
        "WHERE e.course_id = ? AND e.unenrolled_at IS NULL ORDER BY u.full_name");
    st.bind(1, courseId);
    std::vector<UserRecord> out;
    while (st.step()) out.push_back(readUser(st));
    return out;
}

// ===== SqliteAssignmentRepository ===========================================
std::optional<Assignment> SqliteAssignmentRepository::findById(int id) {
    Statement st = db_.prepare(std::string("SELECT ") + kAssignCols + " FROM assignments WHERE assignment_id = ?");
    st.bind(1, id);
    if (!st.step()) return std::nullopt;
    return readAssignment(st);
}

int SqliteAssignmentRepository::insert(const Assignment& a) {
    Statement st = db_.prepare(
        "INSERT INTO assignments (course_id, title, description, deadline, max_mark, is_closed, "
        "created_by, created_at) VALUES (?,?,?,?,?,?,?,?)");
    st.bind(1, a.courseId); st.bind(2, a.title); st.bind(3, a.description);
    st.bind(4, toIso(a.deadline)); st.bind(5, a.maxMark); st.bind(6, a.isClosed);
    st.bind(7, a.createdBy); st.bind(8, toIso(a.createdAt));
    st.step();
    return static_cast<int>(db_.lastInsertId());
}

void SqliteAssignmentRepository::update(const Assignment& a) {
    Statement st = db_.prepare(
        "UPDATE assignments SET title=?, description=?, deadline=?, max_mark=?, is_closed=? "
        "WHERE assignment_id=?");
    st.bind(1, a.title); st.bind(2, a.description); st.bind(3, toIso(a.deadline));
    st.bind(4, a.maxMark); st.bind(5, a.isClosed); st.bind(6, a.id);
    st.step();
}

std::vector<Assignment> SqliteAssignmentRepository::listByCourse(int courseId) {
    Statement st = db_.prepare(std::string("SELECT ") + kAssignCols +
                               " FROM assignments WHERE course_id = ? ORDER BY deadline, assignment_id");
    st.bind(1, courseId);
    std::vector<Assignment> out;
    while (st.step()) out.push_back(readAssignment(st));
    return out;
}

// ===== SqliteExtensionRepository ============================================
std::optional<Extension> SqliteExtensionRepository::latestFor(int assignmentId, int studentId) {
    Statement st = db_.prepare(
        "SELECT extension_id, assignment_id, student_id, new_deadline, reason, granted_by, granted_at "
        "FROM extensions WHERE assignment_id = ? AND student_id = ? "
        "ORDER BY new_deadline DESC, extension_id DESC LIMIT 1");
    st.bind(1, assignmentId); st.bind(2, studentId);
    if (!st.step()) return std::nullopt;
    Extension e;
    e.id = st.columnInt(0); e.assignmentId = st.columnInt(1); e.studentId = st.columnInt(2);
    e.newDeadline = fromIso(st.columnText(3)); e.reason = st.columnText(4);
    e.grantedBy = st.columnInt(5); e.grantedAt = fromIso(st.columnText(6));
    return e;
}

int SqliteExtensionRepository::insert(const Extension& e) {
    Statement st = db_.prepare(
        "INSERT INTO extensions (assignment_id, student_id, new_deadline, reason, granted_by, granted_at) "
        "VALUES (?,?,?,?,?,?)");
    st.bind(1, e.assignmentId); st.bind(2, e.studentId); st.bind(3, toIso(e.newDeadline));
    st.bind(4, e.reason); st.bind(5, e.grantedBy); st.bind(6, toIso(e.grantedAt));
    st.step();
    return static_cast<int>(db_.lastInsertId());
}

// ===== SqliteSubmissionRepository ===========================================
std::optional<Submission> SqliteSubmissionRepository::find(int assignmentId, int studentId) {
    Statement st = db_.prepare(
        "SELECT submission_id, assignment_id, student_id, status FROM submissions "
        "WHERE assignment_id = ? AND student_id = ?");
    st.bind(1, assignmentId); st.bind(2, studentId);
    if (!st.step()) return std::nullopt;
    return Submission{st.columnInt(0), st.columnInt(1), st.columnInt(2), statusFromString(st.columnText(3))};
}

std::optional<Submission> SqliteSubmissionRepository::findById(int id) {
    Statement st = db_.prepare(
        "SELECT submission_id, assignment_id, student_id, status FROM submissions WHERE submission_id = ?");
    st.bind(1, id);
    if (!st.step()) return std::nullopt;
    return Submission{st.columnInt(0), st.columnInt(1), st.columnInt(2), statusFromString(st.columnText(3))};
}

int SqliteSubmissionRepository::insert(const Submission& s) {
    Statement st = db_.prepare("INSERT INTO submissions (assignment_id, student_id, status) VALUES (?,?,?)");
    st.bind(1, s.assignmentId); st.bind(2, s.studentId); st.bind(3, toString(s.status));
    st.step();
    return static_cast<int>(db_.lastInsertId());
}

int SqliteSubmissionRepository::addVersion(const SubmissionVersion& v) {
    Statement st = db_.prepare(
        "INSERT INTO submission_versions (submission_id, version_no, file_name, stored_path, "
        "file_size_bytes, uploaded_at) VALUES (?,?,?,?,?,?)");
    st.bind(1, v.submissionId); st.bind(2, v.versionNo); st.bind(3, v.fileName);
    st.bind(4, v.storedPath); st.bind(5, v.fileSizeBytes); st.bind(6, toIso(v.uploadedAt));
    st.step();
    return static_cast<int>(db_.lastInsertId());
}

namespace {
SubmissionVersion readVersion(const Statement& st) {
    SubmissionVersion v;
    v.id = st.columnInt(0); v.submissionId = st.columnInt(1); v.versionNo = st.columnInt(2);
    v.fileName = st.columnText(3); v.storedPath = st.columnText(4);
    v.fileSizeBytes = st.columnInt64(5); v.uploadedAt = fromIso(st.columnText(6));
    return v;
}
const char* kVersionCols =
    "version_id, submission_id, version_no, file_name, stored_path, file_size_bytes, uploaded_at";
} // namespace

std::optional<SubmissionVersion> SqliteSubmissionRepository::latestVersion(int submissionId) {
    Statement st = db_.prepare(std::string("SELECT ") + kVersionCols +
        " FROM submission_versions WHERE submission_id = ? ORDER BY version_no DESC LIMIT 1");
    st.bind(1, submissionId);
    if (!st.step()) return std::nullopt;
    return readVersion(st);
}

std::vector<SubmissionVersion> SqliteSubmissionRepository::listVersions(int submissionId) {
    Statement st = db_.prepare(std::string("SELECT ") + kVersionCols +
        " FROM submission_versions WHERE submission_id = ? ORDER BY version_no");
    st.bind(1, submissionId);
    std::vector<SubmissionVersion> out;
    while (st.step()) out.push_back(readVersion(st));
    return out;
}

void SqliteSubmissionRepository::updateStatus(int submissionId, SubmissionStatus status) {
    Statement st = db_.prepare("UPDATE submissions SET status = ? WHERE submission_id = ?");
    st.bind(1, toString(status)); st.bind(2, submissionId);
    st.step();
}

// The WHERE clause is built only from fixed text fragments; every value is bound.
std::vector<SubmissionRow> SqliteSubmissionRepository::listRows(const RowFilter& f) {
    std::string sql =
        "SELECT s.submission_id, s.assignment_id, a.course_id, s.student_id, a.title, c.code, "
        "u.full_name, u.student_number, a.max_mark, s.status, v.version_no, v.file_name, v.uploaded_at, "
        "MAX(a.deadline, COALESCE((SELECT MAX(e.new_deadline) FROM extensions e "
        "  WHERE e.assignment_id = s.assignment_id AND e.student_id = s.student_id), a.deadline)), "
        "m.submission_id, m.raw_mark, m.days_late, m.penalty, m.final_mark, m.feedback, m.marked_by, "
        "m.marked_at, m.hod_comment, m.moderated_by, m.moderated_at, m.released_by, m.released_at, "
        "mu.full_name "
        "FROM submissions s "
        "JOIN assignments a ON a.assignment_id = s.assignment_id "
        "JOIN courses c ON c.course_id = a.course_id "
        "JOIN users u ON u.user_id = s.student_id "
        "JOIN submission_versions v ON v.submission_id = s.submission_id AND v.version_no = "
        "  (SELECT MAX(version_no) FROM submission_versions WHERE submission_id = s.submission_id) "
        "LEFT JOIN marks m ON m.submission_id = s.submission_id "
        "LEFT JOIN users mu ON mu.user_id = m.marked_by "
        "WHERE 1 = 1";
    if (f.assignmentId) sql += " AND s.assignment_id = ?";
    if (f.courseId) sql += " AND a.course_id = ?";
    if (f.departmentId) sql += " AND c.department_id = ?";
    if (f.studentId) sql += " AND s.student_id = ?";
    if (f.status) sql += " AND s.status = ?";
    if (f.lecturerId)
        sql += " AND EXISTS (SELECT 1 FROM course_lecturers cl WHERE cl.course_id = a.course_id "
               "AND cl.lecturer_id = ?)";
    sql += " ORDER BY c.code, a.assignment_id, u.full_name";

    Statement st = db_.prepare(sql);
    int i = 1;
    if (f.assignmentId) st.bind(i++, *f.assignmentId);
    if (f.courseId) st.bind(i++, *f.courseId);
    if (f.departmentId) st.bind(i++, *f.departmentId);
    if (f.studentId) st.bind(i++, *f.studentId);
    if (f.status) st.bind(i++, toString(*f.status));
    if (f.lecturerId) st.bind(i++, *f.lecturerId);

    std::vector<SubmissionRow> out;
    while (st.step()) {
        SubmissionRow r;
        r.submissionId = st.columnInt(0); r.assignmentId = st.columnInt(1);
        r.courseId = st.columnInt(2); r.studentId = st.columnInt(3);
        r.assignmentTitle = st.columnText(4); r.courseCode = st.columnText(5);
        r.studentName = st.columnText(6); r.studentNumber = st.columnText(7);
        r.maxMark = st.columnInt(8); r.status = statusFromString(st.columnText(9));
        r.latestVersionNo = st.columnInt(10); r.fileName = st.columnText(11);
        r.uploadedAt = fromIso(st.columnText(12));
        r.effectiveDeadline = fromIso(st.columnText(13));
        if (!st.isNull(14)) {
            r.mark = readMark(st, 14);
            r.markedByName = st.columnText(27);
        }
        out.push_back(std::move(r));
    }
    return out;
}

std::vector<int> SqliteSubmissionRepository::idsByStatus(int assignmentId, SubmissionStatus status) {
    Statement st = db_.prepare(
        "SELECT submission_id FROM submissions WHERE assignment_id = ? AND status = ? ORDER BY submission_id");
    st.bind(1, assignmentId); st.bind(2, toString(status));
    std::vector<int> out;
    while (st.step()) out.push_back(st.columnInt(0));
    return out;
}

bool SqliteSubmissionRepository::hasSubmissions(int assignmentId) {
    Statement st = db_.prepare("SELECT 1 FROM submissions WHERE assignment_id = ? LIMIT 1");
    st.bind(1, assignmentId);
    return st.step();
}

// ===== SqliteMarkRepository ================================================
std::optional<Mark> SqliteMarkRepository::find(int submissionId) {
    Statement st = db_.prepare(std::string("SELECT ") + kMarkCols + " FROM marks WHERE submission_id = ?");
    st.bind(1, submissionId);
    if (!st.step()) return std::nullopt;
    return readMark(st);
}

void SqliteMarkRepository::save(const Mark& m) {
    const bool exists = find(m.submissionId).has_value();
    Statement st = db_.prepare(exists
        ? "UPDATE marks SET raw_mark=?, days_late=?, penalty=?, final_mark=?, feedback=?, marked_by=?, "
          "marked_at=?, hod_comment=?, moderated_by=?, moderated_at=?, released_by=?, released_at=? "
          "WHERE submission_id=?"
        : "INSERT INTO marks (raw_mark, days_late, penalty, final_mark, feedback, marked_by, marked_at, "
          "hod_comment, moderated_by, moderated_at, released_by, released_at, submission_id) "
          "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)");
    st.bind(1, m.rawMark); st.bind(2, m.daysLate); st.bind(3, m.penalty); st.bind(4, m.finalMark);
    st.bind(5, m.feedback); st.bind(6, m.markedBy); st.bind(7, toIso(m.markedAt));
    st.bind(8, m.hodComment); st.bind(9, m.moderatedBy); st.bind(10, isoOpt(m.moderatedAt));
    st.bind(11, m.releasedBy); st.bind(12, isoOpt(m.releasedAt)); st.bind(13, m.submissionId);
    st.step();
}

// Times have one-second resolution, so ">=" is used: a mark released in the same second as the
// previous login is still reported (a missed notice is worse than a rare repeated one).
int SqliteMarkRepository::releasedSince(int studentId, std::optional<TimePoint> since) {
    Statement st = db_.prepare(
        "SELECT COUNT(*) FROM marks m JOIN submissions s ON s.submission_id = m.submission_id "
        "WHERE s.student_id = ? AND m.released_at IS NOT NULL AND m.released_at >= ?");
    st.bind(1, studentId);
    st.bind(2, since ? toIso(*since) : std::string("1970-01-01T00:00:00Z"));
    st.step();
    return st.columnInt(0);
}

// ===== SqliteAuditRepository ===============================================
void SqliteAuditRepository::append(const AuditRecord& r) {
    Statement st = db_.prepare(
        "INSERT INTO audit_records (submission_id, changed_by, changed_at, action_type, field_name, "
        "old_value, new_value, reason) VALUES (?,?,?,?,?,?,?,?)");
    st.bind(1, r.submissionId); st.bind(2, r.changedBy); st.bind(3, toIso(r.changedAt));
    st.bind(4, toString(r.actionType)); st.bind(5, toString(r.fieldName));
    st.bind(6, r.oldValue); st.bind(7, r.newValue); st.bind(8, r.reason);
    st.step();
}

std::vector<AuditRow> SqliteAuditRepository::find(const AuditFilter& f) {
    std::string sql =
        "SELECT r.audit_id, r.submission_id, r.changed_by, r.changed_at, r.action_type, r.field_name, "
        "r.old_value, r.new_value, r.reason, cu.full_name, su.full_name, su.student_number, c.code, a.title "
        "FROM audit_records r "
        "JOIN submissions s ON s.submission_id = r.submission_id "
        "JOIN assignments a ON a.assignment_id = s.assignment_id "
        "JOIN courses c ON c.course_id = a.course_id "
        "JOIN users su ON su.user_id = s.student_id "
        "JOIN users cu ON cu.user_id = r.changed_by "
        "WHERE 1 = 1";
    if (f.departmentId) sql += " AND c.department_id = ?";
    if (f.assignmentId) sql += " AND a.assignment_id = ?";
    if (f.studentId) sql += " AND s.student_id = ?";
    if (f.from) sql += " AND r.changed_at >= ?";
    if (f.to) sql += " AND r.changed_at <= ?";
    sql += " ORDER BY r.changed_at, r.audit_id";

    Statement st = db_.prepare(sql);
    int i = 1;
    if (f.departmentId) st.bind(i++, *f.departmentId);
    if (f.assignmentId) st.bind(i++, *f.assignmentId);
    if (f.studentId) st.bind(i++, *f.studentId);
    if (f.from) st.bind(i++, toIso(*f.from));
    if (f.to) st.bind(i++, toIso(*f.to));

    std::vector<AuditRow> out;
    while (st.step()) {
        AuditRow row;
        AuditRecord& r = row.record;
        r.id = st.columnInt(0); r.submissionId = st.columnInt(1); r.changedBy = st.columnInt(2);
        r.changedAt = fromIso(st.columnText(3));
        r.actionType = auditActionFromString(st.columnText(4));
        r.fieldName = auditFieldFromString(st.columnText(5));
        r.oldValue = st.columnOptText(6); r.newValue = st.columnText(7); r.reason = st.columnText(8);
        row.changedByName = st.columnText(9); row.studentName = st.columnText(10);
        row.studentNumber = st.columnText(11); row.courseCode = st.columnText(12);
        row.assignmentTitle = st.columnText(13);
        out.push_back(std::move(row));
    }
    return out;
}

// ===== SqliteUnitOfWork ====================================================
namespace {
class SqliteTransaction : public ITransaction {
public:
    explicit SqliteTransaction(Database& db) : tx_(db.begin()) {}
    void commit() override { tx_.commit(); }
private:
    Transaction tx_;
};
} // namespace

std::unique_ptr<ITransaction> SqliteUnitOfWork::begin() {
    return std::make_unique<SqliteTransaction>(db_);
}

// ===== SqliteEventLogRepository ============================================
void SqliteEventLogRepository::append(std::optional<int> userId, EventType type,
                                      const std::string& detail, TimePoint at) {
    Statement st = db_.prepare(
        "INSERT INTO event_log (user_id, event_type, detail, occurred_at) VALUES (?,?,?,?)");
    st.bind(1, userId); st.bind(2, toString(type));
    st.bind(3, detail.substr(0, 500)); st.bind(4, toIso(at));
    st.step();
}

} // namespace lms
