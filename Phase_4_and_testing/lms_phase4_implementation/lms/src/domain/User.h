// Abstract User and its four subclasses; UserFactory (Phase 3, Figure 3.2).
#pragma once
#include "domain/Types.h"

#include <memory>
#include <set>
#include <string>

namespace lms {

class User {
public:
    explicit User(const UserRecord& rec) : rec_(rec) {}
    virtual ~User() = default;

    int getId() const { return rec_.id; }
    const std::string& getUsername() const { return rec_.username; }
    const std::string& getFullName() const { return rec_.fullName; }
    bool isActive() const { return rec_.isActive; }
    bool mustChangePassword() const { return rec_.mustChangePassword; }
    bool isLocked(TimePoint now) const { return rec_.lockedUntil && *rec_.lockedUntil > now; }
    std::optional<int> departmentId() const { return rec_.departmentId; }
    void clearMustChangePassword() { rec_.mustChangePassword = false; }

    virtual Role getRole() const = 0;
    virtual std::set<Function> permittedFunctions() const = 0;
    virtual std::string menuTitle() const = 0;

protected:
    UserRecord rec_;
};

class Administrator : public User {
public:
    using User::User;
    Role getRole() const override { return Role::ADMINISTRATOR; }
    std::set<Function> permittedFunctions() const override;
    std::string menuTitle() const override { return "ADMINISTRATOR MENU"; }
};

class Lecturer : public User {
public:
    using User::User;
    Role getRole() const override { return Role::LECTURER; }
    std::set<Function> permittedFunctions() const override;
    std::string menuTitle() const override { return "LECTURER MENU"; }
};

class HeadOfDepartment : public User {
public:
    using User::User;
    Role getRole() const override { return Role::HEAD_OF_DEPARTMENT; }
    std::set<Function> permittedFunctions() const override;
    std::string menuTitle() const override { return "HEAD OF DEPARTMENT MENU"; }
};

class Student : public User {
public:
    using User::User;
    Role getRole() const override { return Role::STUDENT; }
    std::set<Function> permittedFunctions() const override;
    std::string menuTitle() const override { return "STUDENT MENU"; }
    const std::string& studentNumber() const { return number_; }
    Student(const UserRecord& rec) : User(rec), number_(rec.studentNumber.value_or("")) {}
private:
    std::string number_;
};

// The only place that creates user objects (DD-04).
class UserFactory {
public:
    static std::unique_ptr<User> create(const UserRecord& rec);
};

} // namespace lms
