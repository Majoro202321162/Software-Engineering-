// Dependency-free smoke and integration tests for the business and data layers.
// They run against a real in-memory SQLite database. Phase 5 can port them to GoogleTest/Catch2.
#include "business/Services.h"
#include "data/Database.h"
#include "data/SqliteRepositories.h"
#include "infra/Infrastructure.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>

using namespace lms;
namespace fs = std::filesystem;

static int g_checks = 0, g_failed = 0;
#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failed; \
    std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_THROWS(Type, expr) do { ++g_checks; bool ok_ = false; \
    try { expr; } catch (const Type&) { ok_ = true; } catch (...) {} \
    if (!ok_) { ++g_failed; std::printf("  FAIL %s:%d  expected %s from %s\n", __FILE__, __LINE__, #Type, #expr); } } while (0)

namespace {

class FakeHasher : public IPasswordHasher {
public:
    std::string hash(const std::string& pw) override { return "fake$" + pw; }
    bool verify(const std::string& pw, const std::string& h) override { return h == "fake$" + pw; }
    std::string generateTemporaryPassword() override { return "Temp" + std::to_string(++n_) + "abcd"; }
private:
    int n_ = 1000;
};

// Fails on the Nth audit append: used for fault-injection (NFR-09).
class FailingAudit : public IAuditRepository {
public:
    FailingAudit(IAuditRepository& real, int failAt) : real_(real), failAt_(failAt) {}
    void append(const AuditRecord& r) override {
        if (++count_ == failAt_) throw DataAccessException("injected failure");
        real_.append(r);
    }
    std::vector<AuditRow> find(const AuditFilter& f) override { return real_.find(f); }
private:
    IAuditRepository& real_;
    int failAt_, count_ = 0;
};

TimePoint T(const char* iso) { return fromIso(iso); }

struct Env {
    Database db;
    SqliteUserRepository users{db};
    SqliteDepartmentRepository departments{db};
    SqliteCourseRepository courses{db};
    SqliteAssignmentRepository assignments{db};
    SqliteExtensionRepository extensions{db};
    SqliteSubmissionRepository submissions{db};
    SqliteMarkRepository marks{db};
    SqliteAuditRepository audit{db};
    SqliteEventLogRepository events{db};
    SqliteUnitOfWork uow{db};
    RepoSet repos{users, departments, courses, assignments, extensions, submissions, marks, audit, events, uow};
    FixedClock clock{T("2026-11-02T10:00:00Z")};
    FakeHasher hasher;
    PermissionPolicy policy{events, clock};
    TenPercentPerDayPenalty penalty;
    fs::path dir = fs::temp_directory_path() / "lms_test_store";
    ManagedFolderFileStore files{(dir / "store").string()};
    AuthService auth{repos, hasher, clock};
    UserService userSvc{repos, hasher, policy, clock};
    CourseService courseSvc{repos, policy, clock};
    AssignmentService assignSvc{repos, policy, clock};
    AuditService auditSvc{repos, policy, clock};
    SubmissionService subSvc{repos, policy, clock, files, penalty, assignSvc};
    MarkingService markSvc{repos, policy, clock, penalty, assignSvc, auditSvc};
    ModerationService modSvc{repos, policy, clock, penalty, auditSvc};

    // ids
    int deptId = 0, courseId = 0, lecturerId = 0, hodId = 0, stuA = 0, stuB = 0, assignId = 0;
    std::unique_ptr<Session> admin, lecturer, hod, studentA, studentB;
    std::string fileA, adminPw;

    Env() {
        fs::remove_all(dir);
        fs::create_directories(dir);
        db.open(":memory:");
        CreatedUser first = userSvc.createInitialAdministrator();
        adminPw = first.temporaryPassword;
        admin = auth.login("admin", adminPw);
        deptId = courseSvc.createDepartment(*admin, "Science");
        courseId = courseSvc.createCourse(*admin, "CS101", "Intro", deptId);
        NewUser l; l.username = "lect1"; l.fullName = "Lee Lecturer"; l.role = Role::LECTURER; l.departmentId = deptId;
        NewUser h; h.username = "hod1"; h.fullName = "Hana Head"; h.role = Role::HEAD_OF_DEPARTMENT; h.departmentId = deptId;
        NewUser a; a.username = "stud1"; a.fullName = "Sam Student"; a.role = Role::STUDENT; a.studentNumber = "S001";
        NewUser b; b.username = "stud2"; b.fullName = "Sue Student"; b.role = Role::STUDENT; b.studentNumber = "S002";
        CreatedUser cl = userSvc.createUser(*admin, l), ch = userSvc.createUser(*admin, h),
                    ca = userSvc.createUser(*admin, a), cb = userSvc.createUser(*admin, b);
        lecturerId = cl.id; hodId = ch.id; stuA = ca.id; stuB = cb.id;
        courseSvc.assignLecturer(*admin, courseId, lecturerId);
        courseSvc.enrol(*admin, courseId, stuA);
        courseSvc.enrol(*admin, courseId, stuB);
        lecturer = auth.login("lect1", cl.temporaryPassword);
        hod = auth.login("hod1", ch.temporaryPassword);
        studentA = auth.login("stud1", ca.temporaryPassword);
        studentB = auth.login("stud2", cb.temporaryPassword);
        NewAssignment na; na.courseId = courseId; na.title = "Essay 1"; na.description = "Write";
        na.deadline = T("2026-11-10T12:00:00Z"); na.maxMark = 100;
        assignId = assignSvc.createAssignment(*lecturer, na);
        fileA = (dir / "work.txt").string();
        std::ofstream(fileA) << "my answer\nline two\n";
    }
    ~Env() { fs::remove_all(dir); }
    int submissionOf(int studentId) { return submissions.find(assignId, studentId)->id; }
};

// ---------------------------------------------------------------- unit tests
void testPenaltyBoundaries() {
    std::puts("penalty boundaries (Table 3.5)");
    TenPercentPerDayPenalty p;
    const TimePoint d = T("2026-11-10T12:00:00Z");
    auto at = [&](long long secs) { return p.daysLate(d + std::chrono::seconds(secs), d); };
    CHECK(at(-60) == 0); CHECK(at(0) == 0); CHECK(at(1) == 1); CHECK(at(86400) == 1);
    CHECK(at(86401) == 2); CHECK(at(3 * 86400) == 3); CHECK(at(3 * 86400 + 1) == 4);
    CHECK(p.finalMark(80, 100, 0) == 80); CHECK(p.finalMark(80, 100, 1) == 70);
    CHECK(p.finalMark(80, 100, 2) == 60); CHECK(p.finalMark(80, 100, 3) == 50);
    CHECK(p.finalMark(80, 100, 4) == 0); CHECK(p.givesZero(4)); CHECK(!p.givesZero(3));
    CHECK(p.finalMark(5, 100, 1) == 0);                       // never below zero
    CHECK(p.penalty(7, 1) == 0.7); CHECK(p.penalty(7, 3) == 2.1);   // rounding to 2 places
}

void testLifecycleTable() {
    std::puts("life cycle table (Table 4.1)");
    using S = SubmissionStatus;
    const S all[] = {S::SUBMITTED, S::LATE, S::MARKED, S::RETURNED, S::MODERATED, S::RELEASED};
    int allowed = 0;
    for (S a : all) for (S b : all) if (SubmissionLifecycle::isAllowed(a, b)) ++allowed;
    CHECK(allowed == 10);
    CHECK(!SubmissionLifecycle::isAllowed(S::MARKED, S::RELEASED));   // BR-11
    CHECK(!SubmissionLifecycle::isAllowed(S::LATE, S::LATE));
    CHECK(!SubmissionLifecycle::isAllowed(S::MODERATED, S::MARKED));  // FR-22
    CHECK_THROWS(BusinessRuleException, SubmissionLifecycle::require(S::MARKED, S::RELEASED));
}

void testTimeRoundTrip() {
    std::puts("UTC time text");
    CHECK(toIso(T("2026-11-02T14:30:00Z")) == "2026-11-02T14:30:00Z");
    CHECK(toIso(TimePoint(std::chrono::seconds(0))) == "1970-01-01T00:00:00Z");
    CHECK(toIso(T("2028-02-29T23:59:59Z")) == "2028-02-29T23:59:59Z");
    CHECK_THROWS(ValidationException, fromIso("garbage"));
}

void testRealArgon2id() {
    std::puts("libsodium Argon2id hasher");
    LibsodiumArgon2idHasher h;
    std::string a = h.hash("Secret123"), b = h.hash("Secret123");
    CHECK(a != b);                                   // unique random salt each time
    CHECK(a.rfind("$argon2id$", 0) == 0);
    CHECK(h.verify("Secret123", a)); CHECK(!h.verify("secret123", a));
    CHECK(a.find("Secret123") == std::string::npos);
    std::string t = h.generateTemporaryPassword();
    CHECK(t.size() >= 10);
    CHECK_THROWS(ValidationException, AuthService::validateNewPassword("short1"));
    CHECK_THROWS(ValidationException, AuthService::validateNewPassword("lettersonly"));
    AuthService::validateNewPassword("Abcdef12");
}

// ---------------------------------------------------------- integration tests
void testLogin() {
    std::puts("login, lockout, injection (FR-01, FR-02, NFR-06)");
    Env e;
    std::string msg1, msg2;
    try { e.auth.login("nobody", "x"); } catch (const LmsException& ex) { msg1 = ex.userMessage(); }
    try { e.auth.login("stud1", "wrong"); } catch (const LmsException& ex) { msg2 = ex.userMessage(); }
    CHECK(msg1 == "Invalid username or password" && msg1 == msg2);
    CHECK_THROWS(BusinessRuleException, e.auth.login("' OR '1'='1' --", "' OR '1'='1' --"));
    CHECK_THROWS(BusinessRuleException, e.auth.login("stud1'; DROP TABLE users;--", "x"));
    CHECK(e.users.listAll().size() == 5);                    // nothing was dropped
    for (int i = 0; i < 5; ++i) CHECK_THROWS(BusinessRuleException, e.auth.login("stud2", "bad"));
    CHECK_THROWS(BusinessRuleException, e.auth.login("stud2", "Temp1005abcd"));   // locked even with right password
    e.clock.advance(std::chrono::minutes(6));
    CHECK(e.auth.login("stud2", e.userSvc.resetPassword(*e.admin, e.stuB)) != nullptr);
    // inactive account cannot log in
    e.userSvc.setActive(*e.admin, e.stuA, false);
    CHECK_THROWS(BusinessRuleException, e.auth.login("stud1", "anything"));
    // change password (FR-04)
    CHECK_THROWS(BusinessRuleException, e.auth.changePassword(*e.admin, "wrong-current1", "NewPass123"));
    CHECK_THROWS(ValidationException, e.auth.changePassword(*e.admin, e.adminPw, "short1"));
    CHECK_THROWS(ValidationException, e.auth.changePassword(*e.admin, e.adminPw, "nodigitshere"));
    CHECK(e.admin->getUser().mustChangePassword());
    e.auth.changePassword(*e.admin, e.adminPw, "NewPass123");
    CHECK(!e.admin->getUser().mustChangePassword());
    CHECK(e.auth.login("admin", "NewPass123") != nullptr);
    CHECK_THROWS(BusinessRuleException, e.auth.login("admin", e.adminPw));
}

void testPermissions() {
    std::puts("role permissions and scope (FR-03, NFR-07, BR-10, BR-15)");
    Env e;
    CHECK_THROWS(AuthorizationException, e.markSvc.mark(*e.studentA, 1, 50, "some feedback here"));
    CHECK_THROWS(AuthorizationException, e.courseSvc.createDepartment(*e.lecturer, "Hack"));
    CHECK_THROWS(AuthorizationException, e.userSvc.listUsers(*e.studentA));
    CHECK_THROWS(AuthorizationException, e.subSvc.listForLecturer(*e.admin, e.assignId));   // BR-15
    CHECK_THROWS(AuthorizationException, e.modSvc.release(*e.lecturer, 1));
    // another lecturer is outside the course scope
    NewUser l2; l2.username = "lect2"; l2.fullName = "Other Lect"; l2.role = Role::LECTURER; l2.departmentId = e.deptId;
    CreatedUser c2 = e.userSvc.createUser(*e.admin, l2);
    auto other = e.auth.login("lect2", c2.temporaryPassword);
    CHECK_THROWS(AuthorizationException, e.subSvc.listForLecturer(*other, e.assignId));
    NewAssignment na; na.courseId = e.courseId; na.title = "X"; na.description = ""; na.deadline = T("2026-12-01T00:00:00Z"); na.maxMark = 10;
    CHECK_THROWS(AuthorizationException, e.assignSvc.createAssignment(*other, na));
    // a student outside the course cannot submit
    NewUser s3; s3.username = "stud3"; s3.fullName = "No Course"; s3.role = Role::STUDENT; s3.studentNumber = "S003";
    CreatedUser c3 = e.userSvc.createUser(*e.admin, s3);
    auto stranger = e.auth.login("stud3", c3.temporaryPassword);
    CHECK_THROWS(AuthorizationException, e.subSvc.preview(*stranger, e.assignId, e.fileA));
    // refusals are logged
    Statement st = e.db.prepare("SELECT COUNT(*) FROM event_log WHERE event_type = 'ACCESS_DENIED'");
    st.step();
    CHECK(st.columnInt(0) >= 6);
    // one HoD per department
    NewUser h2; h2.username = "hod2"; h2.fullName = "Second Head"; h2.role = Role::HEAD_OF_DEPARTMENT; h2.departmentId = e.deptId;
    CHECK_THROWS(BusinessRuleException, e.userSvc.createUser(*e.admin, h2));
}

void testSubmissionPaths() {
    std::puts("submit assignment paths P1-P7 (FR-15 to FR-18)");
    Env e;
    // P5 invalid files
    CHECK_THROWS(FileException, e.subSvc.preview(*e.studentA, e.assignId, (e.dir / "missing.txt").string()));
    std::ofstream((e.dir / "empty.txt").string()).close();
    CHECK_THROWS(FileException, e.subSvc.preview(*e.studentA, e.assignId, (e.dir / "empty.txt").string()));
    std::ofstream((e.dir / "bad.exe").string()) << "x";
    CHECK_THROWS(FileException, e.subSvc.preview(*e.studentA, e.assignId, (e.dir / "bad.exe").string()));
    CHECK(!e.submissions.find(e.assignId, e.stuA));
    // P1 on time
    e.clock.set(T("2026-11-09T09:00:00Z"));
    auto pv = e.subSvc.preview(*e.studentA, e.assignId, e.fileA);
    CHECK(!pv.late && !pv.isResubmission);
    Receipt r1 = e.subSvc.submit(*e.studentA, e.assignId, e.fileA, false);
    CHECK(r1.versionNo == 1 && r1.status == SubmissionStatus::SUBMITTED);
    // P2 resubmit keeps older versions
    e.clock.set(T("2026-11-10T11:59:59Z"));
    Receipt r2 = e.subSvc.submit(*e.studentA, e.assignId, e.fileA, false);
    CHECK(r2.versionNo == 2);
    CHECK(e.submissions.listVersions(r1.submissionId).size() == 2);
    // exactly at the deadline is still on time (BR-04)
    e.clock.set(T("2026-11-10T12:00:00Z"));
    CHECK(e.subSvc.submit(*e.studentA, e.assignId, e.fileA, false).versionNo == 3);
    // P6 resubmission after the deadline refused
    e.clock.set(T("2026-11-10T12:00:01Z"));
    CHECK_THROWS(BusinessRuleException, e.subSvc.submit(*e.studentA, e.assignId, e.fileA, false));
    // P3 / P4 late: needs confirmation
    auto pl = e.subSvc.preview(*e.studentB, e.assignId, e.fileA);
    CHECK(pl.late && pl.daysLate == 1 && pl.penaltyMarks == 10 && !pl.zeroMark);
    CHECK_THROWS(BusinessRuleException, e.subSvc.submit(*e.studentB, e.assignId, e.fileA, false));
    CHECK(!e.submissions.find(e.assignId, e.stuB));                   // P4: nothing saved
    Receipt late = e.subSvc.submit(*e.studentB, e.assignId, e.fileA, true);
    CHECK(late.status == SubmissionStatus::LATE);
    CHECK_THROWS(BusinessRuleException, e.subSvc.submit(*e.studentB, e.assignId, e.fileA, true));  // late cannot resubmit
    // closed assignment
    AssignmentEdit ed; ed.close = true;
    e.assignSvc.editAssignment(*e.lecturer, e.assignId, ed);
    CHECK_THROWS(BusinessRuleException, e.subSvc.preview(*e.studentA, e.assignId, e.fileA));
}

void testExtension() {
    std::puts("extensions and effective deadline (CF2, BR-03, BR-09, T4)");
    Env e;
    e.clock.set(T("2026-11-11T12:00:00Z"));                      // one day after the deadline
    Receipt r = e.subSvc.submit(*e.studentA, e.assignId, e.fileA, true);
    CHECK(r.status == SubmissionStatus::LATE);
    CHECK_THROWS(ValidationException, e.assignSvc.grantExtension(*e.lecturer, e.assignId, e.stuA, T("2026-11-01T00:00:00Z"), "long enough reason"));
    CHECK_THROWS(ValidationException, e.assignSvc.grantExtension(*e.lecturer, e.assignId, e.stuA, T("2026-11-20T00:00:00Z"), "short"));
    e.assignSvc.grantExtension(*e.lecturer, e.assignId, e.stuA, T("2026-11-12T00:00:00Z"), "Family emergency");
    CHECK(e.submissions.find(e.assignId, e.stuA)->status == SubmissionStatus::SUBMITTED);   // T4
    CHECK(e.assignSvc.effectiveDeadline(e.assignId, e.stuA) == T("2026-11-12T00:00:00Z"));
    CHECK(e.assignSvc.effectiveDeadline(e.assignId, e.stuB) == T("2026-11-10T12:00:00Z"));
    MarkResult m = e.markSvc.mark(*e.lecturer, r.submissionId, 80, "Well done overall");
    CHECK(m.daysLate == 0 && m.finalMark == 80);                 // inside the extension: not late
    CHECK_THROWS(BusinessRuleException, e.assignSvc.grantExtension(*e.lecturer, e.assignId, e.stuA, T("2026-11-30T00:00:00Z"), "after marking"));
}

void testMarkingAndAudit() {
    std::puts("marking, penalty, re-marking and audit (FR-19, FR-21, FR-22, FR-29)");
    Env e;
    e.clock.set(T("2026-11-10T12:00:01Z"));
    int sa = e.subSvc.submit(*e.studentA, e.assignId, e.fileA, true).submissionId;      // 1 s late
    CHECK_THROWS(ValidationException, e.markSvc.mark(*e.lecturer, sa, 80.3, "valid feedback text"));
    CHECK_THROWS(ValidationException, e.markSvc.mark(*e.lecturer, sa, 101, "valid feedback text"));
    CHECK_THROWS(ValidationException, e.markSvc.mark(*e.lecturer, sa, 80, "short"));
    e.clock.set(T("2026-11-12T00:00:00Z"));
    MarkResult m = e.markSvc.mark(*e.lecturer, sa, 80, "Good analysis, late though");
    CHECK(m.daysLate == 1 && m.penalty == 10 && m.finalMark == 70);
    CHECK(e.submissions.findById(sa)->status == SubmissionStatus::MARKED);
    auto audits = e.audit.find(AuditFilter{});
    CHECK(audits.size() == 3);
    CHECK(audits[0].record.actionType == AuditAction::INITIAL_MARKING && !audits[0].record.oldValue);
    CHECK(audits[0].record.reason == "Initial marking");
    CHECK_THROWS(BusinessRuleException, e.markSvc.mark(*e.lecturer, sa, 60, "second attempt text"));   // use re-mark
    CHECK_THROWS(ValidationException, e.markSvc.remark(*e.lecturer, sa, 90, "Updated feedback here", "x"));
    CHECK_THROWS(ValidationException, e.markSvc.remark(*e.lecturer, sa, 80, "Good analysis, late though", "no change"));
    MarkResult m2 = e.markSvc.remark(*e.lecturer, sa, 90, "Good analysis, late though", "Missed a page earlier");
    CHECK(m2.finalMark == 80);
    audits = e.audit.find(AuditFilter{});
    CHECK(audits.size() == 5);                                    // RAW_MARK and FINAL_MARK added
    CHECK(audits[3].record.oldValue && *audits[3].record.oldValue == "80" && audits[3].record.newValue == "90");
    // 4 days late = zero but still marked with feedback (CF3, BR-07)
    e.clock.set(T("2026-11-15T00:00:00Z"));
    int sb = e.subSvc.submit(*e.studentB, e.assignId, e.fileA, true).submissionId;
    MarkResult z = e.markSvc.mark(*e.lecturer, sb, 95, "Excellent work but very late");
    CHECK(z.daysLate == 5 && z.finalMark == 0 && z.rawMark == 95);
    // audit is append-only, enforced by triggers too (FR-29)
    CHECK_THROWS(DataAccessException, e.db.exec("UPDATE audit_records SET reason = 'tampered record'"));
    CHECK_THROWS(DataAccessException, e.db.exec("DELETE FROM audit_records"));
    CHECK_THROWS(DataAccessException, e.db.exec("DELETE FROM marks"));
    CHECK_THROWS(DataAccessException, e.db.exec("DELETE FROM users"));
    CHECK_THROWS(DataAccessException, e.db.exec("UPDATE marks SET final_mark = 500"));   // above max
}

void testFaultInjection() {
    std::puts("mark and audit commit together or not at all (NFR-09)");
    Env e;
    e.clock.set(T("2026-11-09T00:00:00Z"));
    int sa = e.subSvc.submit(*e.studentA, e.assignId, e.fileA, false).submissionId;
    for (int failAt = 1; failAt <= 3; ++failAt) {
        FailingAudit failing(e.audit, failAt);
        RepoSet faulty{e.users, e.departments, e.courses, e.assignments, e.extensions, e.submissions,
                       e.marks, failing, e.events, e.uow};
        AuditService auditF(faulty, e.policy, e.clock);
        MarkingService markF(faulty, e.policy, e.clock, e.penalty, e.assignSvc, auditF);
        CHECK_THROWS(DataAccessException, markF.mark(*e.lecturer, sa, 70, "feedback that is long enough"));
        CHECK(!e.marks.find(sa));                                 // no mark without its audit record
        CHECK(e.submissions.findById(sa)->status == SubmissionStatus::SUBMITTED);
        CHECK(e.audit.find(AuditFilter{}).empty());
    }
    e.markSvc.mark(*e.lecturer, sa, 70, "feedback that is long enough");
    CHECK(e.audit.find(AuditFilter{}).size() == 3);
}

void testModerationAndRelease() {
    std::puts("moderation, release and student visibility (CF1, FR-24 to FR-27, BR-02, BR-10, BR-11)");
    Env e;
    e.clock.set(T("2026-11-09T00:00:00Z"));
    int sa = e.subSvc.submit(*e.studentA, e.assignId, e.fileA, false).submissionId;
    int sb = e.subSvc.submit(*e.studentB, e.assignId, e.fileA, false).submissionId;
    e.markSvc.mark(*e.lecturer, sa, 75, "Solid piece of work");
    e.markSvc.mark(*e.lecturer, sb, 60, "Needs more references");
    // student sees only a status while unreleased
    auto v = e.subSvc.listForStudent(*e.studentA);
    CHECK(v.size() == 1 && !v[0].released && v[0].statusText == "Awaiting release" && v[0].feedback.empty());
    // cannot release before moderation (BR-11)
    CHECK_THROWS(BusinessRuleException, e.modSvc.release(*e.hod, sa));
    CHECK(e.modSvc.countAwaitingModeration(*e.hod, e.assignId) == 2);
    // return for remarking, lecturer re-marks, then HoD approves
    e.modSvc.returnForRemarking(*e.hod, sb, "Check the second question");
    CHECK(e.submissions.findById(sb)->status == SubmissionStatus::RETURNED);
    CHECK(e.marks.find(sb)->hodComment.value() == "Check the second question");
    e.markSvc.remark(*e.lecturer, sb, 65, "Needs more references", "HoD asked for a check");
    CHECK(e.submissions.findById(sb)->status == SubmissionStatus::MARKED);
    // adjust needs a reason, new value and keeps audit
    CHECK_THROWS(ValidationException, e.modSvc.adjust(*e.hod, sa, 80, "x"));
    CHECK_THROWS(ValidationException, e.modSvc.adjust(*e.hod, sa, 75, "same mark as before"));
    e.modSvc.adjust(*e.hod, sa, 78, "Moderated up after review");
    CHECK(e.marks.find(sa)->finalMark == 78 && e.submissions.findById(sa)->status == SubmissionStatus::MODERATED);
    CHECK(e.modSvc.approveAll(*e.hod, e.assignId) == 1);          // only sb is still Marked
    CHECK(e.modSvc.countReleasable(*e.hod, e.assignId) == 2);
    // a lecturer cannot change a moderated mark (FR-22)
    CHECK_THROWS(BusinessRuleException, e.markSvc.remark(*e.lecturer, sa, 50, "Trying to change it", "lecturer changes"));
    // still hidden before release
    CHECK(!e.subSvc.listForStudent(*e.studentA)[0].released);
    CHECK(e.subSvc.releasedSinceLastLogin(*e.studentA) == 0);
    e.clock.advance(std::chrono::minutes(1));
    CHECK(e.modSvc.releaseAll(*e.hod, e.assignId) == 2);
    v = e.subSvc.listForStudent(*e.studentA);
    CHECK(v[0].released && v[0].finalMark == 78 && v[0].feedback == "Solid piece of work");
    CHECK(e.subSvc.releasedSinceLastLogin(*e.studentA) == 1);
    // post-release correction keeps status Released with audit (FR-35)
    e.modSvc.correctReleased(*e.hod, sa, 80, "Arithmetic error found");
    CHECK(e.marks.find(sa)->finalMark == 80 && e.submissions.findById(sa)->status == SubmissionStatus::RELEASED);
    auto a = e.audit.find(AuditFilter{});
    CHECK(a.back().record.actionType == AuditAction::POST_RELEASE_CORRECTION);
    // HoD audit view is limited to the own department (FR-30)
    CHECK(!e.auditSvc.query(*e.hod, AuditFilter{}).empty());
    CHECK_THROWS(AuthorizationException, e.auditSvc.query(*e.lecturer, AuditFilter{}));
}

void testHodCannotModerateOwnMark() {
    std::puts("separation of marking and moderation (BR-10)");
    Env e;
    e.clock.set(T("2026-11-09T00:00:00Z"));
    int sa = e.subSvc.submit(*e.studentA, e.assignId, e.fileA, false).submissionId;
    e.markSvc.mark(*e.lecturer, sa, 75, "Solid piece of work");
    // simulate a mark entered by the HoD's own account
    e.db.exec("UPDATE marks SET marked_by = " + std::to_string(e.hodId));
    CHECK_THROWS(BusinessRuleException, e.modSvc.approve(*e.hod, sa));
    // a HoD of another department cannot reach the course
    int d2 = e.courseSvc.createDepartment(*e.admin, "Arts");
    NewUser h2; h2.username = "hod2"; h2.fullName = "Arts Head"; h2.role = Role::HEAD_OF_DEPARTMENT; h2.departmentId = d2;
    CreatedUser c = e.userSvc.createUser(*e.admin, h2);
    auto otherHod = e.auth.login("hod2", c.temporaryPassword);
    CHECK_THROWS(AuthorizationException, e.modSvc.approve(*otherHod, sa));
    CHECK(e.modSvc.listForDepartment(*otherHod, RowFilter{}).empty());
}

void testEnrolmentHistory() {
    std::puts("unenrolling keeps submissions (FR-10, BR-13)");
    Env e;
    e.clock.set(T("2026-11-09T00:00:00Z"));
    int sa = e.subSvc.submit(*e.studentA, e.assignId, e.fileA, false).submissionId;
    e.courseSvc.unenrol(*e.admin, e.courseId, e.stuA);
    CHECK(e.submissions.findById(sa).has_value());
    CHECK(!e.courses.isEnrolled(e.courseId, e.stuA));
    CHECK_THROWS(AuthorizationException, e.subSvc.preview(*e.studentA, e.assignId, e.fileA));
    e.courseSvc.enrol(*e.admin, e.courseId, e.stuA);               // can come back
    CHECK(e.courses.isEnrolled(e.courseId, e.stuA));
}

void testBackupAndExport() {
    std::puts("backup and CSV export (FR-33, FR-34)");
    Env e;
    e.clock.set(T("2026-11-09T00:00:00Z"));
    int sa = e.subSvc.submit(*e.studentA, e.assignId, e.fileA, false).submissionId;
    e.markSvc.mark(*e.lecturer, sa, 75, "Solid piece of work");
    e.modSvc.approve(*e.hod, sa);
    e.modSvc.release(*e.hod, sa);
    BackupService backup(e.uow, e.files, e.policy, e.clock);
    std::string out = backup.backup(*e.admin, (e.dir / "backups").string());
    CHECK(fs::exists(fs::path(out) / "lms.db"));
    CHECK(fs::exists(fs::path(out) / "submissions"));
    CHECK_THROWS(AuthorizationException, backup.backup(*e.lecturer, (e.dir / "b2").string()));
    ReportExporter exporter(e.repos, e.policy);
    std::string csv = (e.dir / "marks.csv").string();
    CHECK(exporter.exportReleasedMarks(*e.lecturer, e.assignId, csv) == 1);
    std::ifstream in(csv);
    std::string header, line;
    std::getline(in, header); std::getline(in, line);
    CHECK(header.find("final_mark") != std::string::npos && line.find("S001") != std::string::npos);
}

} // namespace

int main() {
    testPenaltyBoundaries();
    testLifecycleTable();
    testTimeRoundTrip();
    testRealArgon2id();
    testLogin();
    testPermissions();
    testSubmissionPaths();
    testExtension();
    testMarkingAndAudit();
    testFaultInjection();
    testModerationAndRelease();
    testHodCannotModerateOwnMark();
    testEnrolmentHistory();
    testBackupAndExport();
    std::printf("\n%d checks, %d failed\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}
