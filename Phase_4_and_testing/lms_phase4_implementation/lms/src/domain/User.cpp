#include "domain/User.h"
#include "common/Exceptions.h"

namespace lms {

std::set<Function> Administrator::permittedFunctions() const {
    return {Function::CHANGE_PASSWORD, Function::MANAGE_ACCOUNTS,
            Function::MANAGE_STRUCTURE, Function::BACK_UP_DATA};
}

std::set<Function> Lecturer::permittedFunctions() const {
    return {Function::CHANGE_PASSWORD, Function::MANAGE_ASSIGNMENTS, Function::GRANT_EXTENSION,
            Function::VIEW_SUBMISSIONS, Function::MARK_SUBMISSION, Function::EXPORT_MARKS};
}

std::set<Function> HeadOfDepartment::permittedFunctions() const {
    return {Function::CHANGE_PASSWORD, Function::VIEW_SUBMISSIONS, Function::MODERATE_MARKS,
            Function::RELEASE_MARKS, Function::CORRECT_MARK, Function::VIEW_AUDIT_LOG,
            Function::EXPORT_MARKS};
}

std::set<Function> Student::permittedFunctions() const {
    return {Function::CHANGE_PASSWORD, Function::VIEW_COURSES, Function::SUBMIT_WORK,
            Function::VIEW_RELEASED_MARKS};
}

std::unique_ptr<User> UserFactory::create(const UserRecord& rec) {
    switch (roleFromString(rec.role)) {
        case Role::ADMINISTRATOR: return std::make_unique<Administrator>(rec);
        case Role::LECTURER: return std::make_unique<Lecturer>(rec);
        case Role::HEAD_OF_DEPARTMENT: return std::make_unique<HeadOfDepartment>(rec);
        case Role::STUDENT: return std::make_unique<Student>(rec);
    }
    throw ValidationException("Unknown role");
}

} // namespace lms
