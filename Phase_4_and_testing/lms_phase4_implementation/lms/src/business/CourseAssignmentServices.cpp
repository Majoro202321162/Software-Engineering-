#include "business/Services.h"

namespace lms {

// ===== CourseService ========================================================
int CourseService::createDepartment(const Session& s, const std::string& rawName) {
    p_.require(s.getUser(), Function::MANAGE_STRUCTURE);
    const std::string name = trim(rawName);
    requireLength("Department name", name, 2, 100);
    if (r_.departments.findByName(name)) throw BusinessRuleException("That department already exists.");
    return r_.departments.insert(name);
}

void CourseService::renameDepartment(const Session& s, int id, const std::string& rawName) {
    p_.require(s.getUser(), Function::MANAGE_STRUCTURE);
    const std::string name = trim(rawName);
    requireLength("Department name", name, 2, 100);
    if (!r_.departments.findById(id)) throw ValidationException("That department does not exist.");
    auto other = r_.departments.findByName(name);
    if (other && other->id != id) throw BusinessRuleException("That department name is already used.");
    r_.departments.rename(id, name);
}

std::vector<Department> CourseService::listDepartments(const Session& s) {
    p_.require(s.getUser(), Function::MANAGE_STRUCTURE);
    return r_.departments.listAll();
}

int CourseService::createCourse(const Session& s, const std::string& rawCode, const std::string& rawTitle,
                                int departmentId) {
    p_.require(s.getUser(), Function::MANAGE_STRUCTURE);
    Course c;
    c.code = trim(rawCode);
    c.title = trim(rawTitle);
    requireLength("Course code", c.code, 2, 20);
    requireLength("Course title", c.title, 2, 100);
    if (!r_.departments.findById(departmentId)) throw ValidationException("That department does not exist.");
    if (r_.courses.findByCode(c.code)) throw BusinessRuleException("That course code is already used.");
    c.departmentId = departmentId;
    return r_.courses.insert(c);
}

void CourseService::editCourse(const Session& s, int courseId, const std::string& rawTitle, bool isActive) {
    p_.require(s.getUser(), Function::MANAGE_STRUCTURE);
    auto c = r_.courses.findById(courseId);
    if (!c) throw ValidationException("That course does not exist.");
    c->title = trim(rawTitle);
    requireLength("Course title", c->title, 2, 100);
    c->isActive = isActive;                    // courses are closed, never deleted (BR-13)
    r_.courses.update(*c);
}

std::vector<Course> CourseService::listAllCourses(const Session& s) {
    p_.require(s.getUser(), Function::MANAGE_STRUCTURE);
    return r_.courses.listAll();
}

std::vector<Course> CourseService::coursesFor(const Session& s) {
    const User& u = s.getUser();
    switch (u.getRole()) {
        case Role::LECTURER:
            p_.require(u, Function::MANAGE_ASSIGNMENTS);
            return r_.courses.listByLecturer(u.getId());
        case Role::HEAD_OF_DEPARTMENT:
            p_.require(u, Function::MODERATE_MARKS);
            return r_.courses.listByDepartment(AccessScope(r_, p_).hodDepartment(s));
        case Role::STUDENT:
            p_.require(u, Function::VIEW_COURSES);
            return r_.courses.listByStudent(u.getId());
        case Role::ADMINISTRATOR:
            break;
    }
    p_.deny(u, "coursesFor refused for administrator");
}

void CourseService::assignLecturer(const Session& s, int courseId, int userId) {
    p_.require(s.getUser(), Function::MANAGE_STRUCTURE);
    auto c = r_.courses.findById(courseId);
    auto u = r_.users.findById(userId);
    if (!c) throw ValidationException("That course does not exist.");
    if (!u || u->role != toString(Role::LECTURER) || !u->isActive)
        throw ValidationException("That user is not an active lecturer.");
    if (u->departmentId != c->departmentId)
        throw BusinessRuleException("The lecturer belongs to a different department than the course.");
    r_.courses.addLecturer(courseId, userId);
}

void CourseService::removeLecturer(const Session& s, int courseId, int userId) {
    p_.require(s.getUser(), Function::MANAGE_STRUCTURE);
    if (!r_.courses.isLecturerOf(courseId, userId))
        throw ValidationException("That lecturer is not assigned to the course.");
    r_.courses.removeLecturer(courseId, userId);
}

void CourseService::enrol(const Session& s, int courseId, int studentId) {
    p_.require(s.getUser(), Function::MANAGE_STRUCTURE);
    auto c = r_.courses.findById(courseId);
    auto u = r_.users.findById(studentId);
    if (!c || !c->isActive) throw ValidationException("That course does not exist or is closed.");
    if (!u || u->role != toString(Role::STUDENT) || !u->isActive)
        throw ValidationException("That user is not an active student.");
    if (r_.courses.isEnrolled(courseId, studentId))
        throw BusinessRuleException("The student is already enrolled in this course.");
    r_.courses.setEnrolment(courseId, studentId, true, clock_.now());
}

void CourseService::unenrol(const Session& s, int courseId, int studentId) {
    p_.require(s.getUser(), Function::MANAGE_STRUCTURE);
    if (!r_.courses.isEnrolled(courseId, studentId))
        throw ValidationException("The student is not enrolled in this course.");
    r_.courses.setEnrolment(courseId, studentId, false, clock_.now());
}

std::vector<UserRecord> CourseService::lecturersOf(const Session& s, int courseId) {
    p_.require(s.getUser(), Function::MANAGE_STRUCTURE);
    return r_.courses.lecturersOf(courseId);
}

std::vector<UserRecord> CourseService::studentsOf(const Session& s, int courseId) {
    p_.require(s.getUser(), Function::MANAGE_STRUCTURE);
    return r_.courses.studentsOf(courseId);
}

// ===== AssignmentService ====================================================
int AssignmentService::createAssignment(const Session& s, const NewAssignment& data) {
    p_.require(s.getUser(), Function::MANAGE_ASSIGNMENTS);
    Course course = scope_.lecturerCourse(s, data.courseId);
    if (!course.isActive) throw BusinessRuleException("That course is closed.");
    Assignment a;
    a.courseId = course.id;
    a.title = trim(data.title);
    a.description = trim(data.description);
    requireLength("Title", a.title, 2, 100);
    if (a.description.size() > 2000) throw ValidationException("The description is longer than 2000 characters.");
    if (data.maxMark < 1 || data.maxMark > 100)
        throw ValidationException("The maximum mark must be a whole number from 1 to 100.");
    if (data.deadline <= clock_.now()) throw ValidationException("The deadline must be in the future.");
    a.deadline = data.deadline;
    a.maxMark = data.maxMark;
    a.createdBy = s.getUser().getId();
    a.createdAt = clock_.now();
    return r_.assignments.insert(a);
}

void AssignmentService::editAssignment(const Session& s, int assignmentId, const AssignmentEdit& edit) {
    p_.require(s.getUser(), Function::MANAGE_ASSIGNMENTS);
    Assignment a = scope_.assignment(assignmentId);
    scope_.lecturerCourse(s, a.courseId);
    if (edit.title) {
        a.title = trim(*edit.title);
        requireLength("Title", a.title, 2, 100);
    }
    if (edit.description) {
        a.description = trim(*edit.description);
        if (a.description.size() > 2000) throw ValidationException("The description is longer than 2000 characters.");
    }
    if (edit.deadline) {
        if (*edit.deadline <= a.deadline)
            throw BusinessRuleException("The deadline can only be moved later, never earlier (FR-12).");
        a.deadline = *edit.deadline;
    }
    if (edit.maxMark) {
        if (r_.submissions.hasSubmissions(a.id))
            throw BusinessRuleException("The maximum mark cannot change once a submission exists.");
        if (*edit.maxMark < 1 || *edit.maxMark > 100)
            throw ValidationException("The maximum mark must be a whole number from 1 to 100.");
        a.maxMark = *edit.maxMark;
    }
    if (edit.close) a.isClosed = *edit.close;
    r_.assignments.update(a);
}

TimePoint AssignmentService::effectiveDeadline(int assignmentId, int studentId) {
    Assignment a = scope_.assignment(assignmentId);
    auto ext = r_.extensions.latestFor(assignmentId, studentId);
    return (ext && ext->newDeadline > a.deadline) ? ext->newDeadline : a.deadline;   // BR-03
}

// FR-13, BR-09: allowed until the student's work is marked; T4 moves Late back to Submitted
// when the new deadline covers the time the work was handed in.
void AssignmentService::grantExtension(const Session& s, int assignmentId, int studentId,
                                       TimePoint newDeadline, const std::string& rawReason) {
    p_.require(s.getUser(), Function::GRANT_EXTENSION);
    Assignment a = scope_.assignment(assignmentId);
    scope_.lecturerCourse(s, a.courseId);
    const std::string reason = trim(rawReason);
    if (reason.size() < 10 || reason.size() > 500)
        throw ValidationException("The reason must be 10 to 500 characters long.");
    if (a.isClosed) throw BusinessRuleException("The assignment is closed.");
    if (!r_.courses.isEnrolled(a.courseId, studentId))
        throw ValidationException("That student is not enrolled in the course.");
    const TimePoint current = effectiveDeadline(assignmentId, studentId);
    if (newDeadline <= current)
        throw ValidationException("The new deadline must be later than the current deadline of " +
                                  toLocalDisplay(current) + ".");

    auto sub = r_.submissions.find(assignmentId, studentId);
    if (sub && sub->status != SubmissionStatus::SUBMITTED && sub->status != SubmissionStatus::LATE)
        throw BusinessRuleException("An extension is only possible until the work is marked.");

    auto tx = r_.uow.begin();
    Extension e;
    e.assignmentId = assignmentId;
    e.studentId = studentId;
    e.newDeadline = newDeadline;
    e.reason = reason;
    e.grantedBy = s.getUser().getId();
    e.grantedAt = clock_.now();
    r_.extensions.insert(e);
    if (sub && sub->status == SubmissionStatus::LATE) {
        auto version = r_.submissions.latestVersion(sub->id);
        if (version && version->uploadedAt <= newDeadline) {            // T4
            SubmissionLifecycle::require(SubmissionStatus::LATE, SubmissionStatus::SUBMITTED);
            r_.submissions.updateStatus(sub->id, SubmissionStatus::SUBMITTED);
        }
    }
    tx->commit();
}

std::vector<Assignment> AssignmentService::listByCourse(const Session& s, int courseId) {
    const User& u = s.getUser();
    auto c = r_.courses.findById(courseId);
    if (!c) throw ValidationException("That course does not exist.");
    switch (u.getRole()) {
        case Role::LECTURER:
            p_.require(u, Function::MANAGE_ASSIGNMENTS);
            scope_.lecturerCourse(s, courseId);
            break;
        case Role::HEAD_OF_DEPARTMENT:
            p_.require(u, Function::VIEW_SUBMISSIONS);
            scope_.departmentCourse(s, courseId);
            break;
        case Role::STUDENT:
            p_.require(u, Function::VIEW_COURSES);
            if (!r_.courses.isEnrolled(courseId, u.getId())) p_.deny(u, "Not enrolled in course");
            break;
        case Role::ADMINISTRATOR:
            p_.deny(u, "Administrator cannot list assignments");
    }
    return r_.assignments.listByCourse(courseId);
}

std::vector<UserRecord> AssignmentService::enrolledStudents(const Session& s, int assignmentId) {
    p_.require(s.getUser(), Function::GRANT_EXTENSION);
    Assignment a = scope_.assignment(assignmentId);
    scope_.lecturerCourse(s, a.courseId);
    return r_.courses.studentsOf(a.courseId);
}

} // namespace lms
