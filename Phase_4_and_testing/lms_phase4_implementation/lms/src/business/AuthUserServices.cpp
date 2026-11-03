#include "business/Services.h"

#include <cctype>

namespace lms {
namespace {
const char* kInvalidLogin = "Invalid username or password";    // same text for both failures (FR-02)
}

// ===== AuthService ==========================================================
AuthService::AuthService(const RepoSet& r, IPasswordHasher& hasher, IClock& clock)
    : r_(r), hasher_(hasher), clock_(clock), dummyHash_(hasher.hash("Not-a-real-password-1")) {}

void AuthService::validateNewPassword(const std::string& pw) {
    if (pw.size() < 8 || pw.size() > 128)
        throw ValidationException("The password must be 8 to 128 characters long.");
    bool letter = false, digit = false;
    for (unsigned char c : pw) {
        if (std::isalpha(c)) letter = true;
        if (std::isdigit(c)) digit = true;
    }
    if (!letter || !digit)
        throw ValidationException("The password must contain at least one letter and one digit.");
}

std::unique_ptr<Session> AuthService::login(const std::string& rawUsername, const std::string& password) {
    const TimePoint now = clock_.now();
    const std::string username = trim(rawUsername);
    std::optional<UserRecord> rec;
    if (!username.empty() && username.size() <= 30) rec = r_.users.findByUsername(username);

    // Unknown or deactivated account: check a dummy hash so both failures take similar time.
    if (!rec || !rec->isActive) {
        hasher_.verify(password, dummyHash_);
        r_.events.append(rec ? std::optional<int>(rec->id) : std::nullopt, EventType::LOGIN_FAILED,
                         rec ? "Login refused: account deactivated" : "Login refused: unknown username", now);
        throw BusinessRuleException(kInvalidLogin);
    }

    if (rec->lockedUntil && *rec->lockedUntil > now) {
        const auto secs = std::chrono::duration_cast<std::chrono::seconds>(*rec->lockedUntil - now).count();
        r_.events.append(rec->id, EventType::LOGIN_LOCKED, "Login refused: account locked", now);
        throw BusinessRuleException("This account is locked after repeated failed logins. Try again in " +
                                    std::to_string((secs + 59) / 60) + " minute(s).");
    }

    if (!hasher_.verify(password, rec->passwordHash)) {
        rec->failedAttempts += 1;
        if (rec->failedAttempts >= kMaxFailedAttempts) {
            rec->lockedUntil = now + std::chrono::minutes(kLockMinutes);
            rec->failedAttempts = 0;
            r_.events.append(rec->id, EventType::LOGIN_LOCKED, "Account locked after 5 failed logins", now);
        } else {
            r_.events.append(rec->id, EventType::LOGIN_FAILED, "Wrong password", now);
        }
        r_.users.update(*rec);
        throw BusinessRuleException(kInvalidLogin);
    }

    const std::optional<TimePoint> previous = rec->lastLoginAt;
    rec->failedAttempts = 0;
    rec->lockedUntil.reset();
    rec->lastLoginAt = now;
    r_.users.update(*rec);
    r_.events.append(rec->id, EventType::LOGIN_OK, "Login", now);
    return std::make_unique<Session>(UserFactory::create(*rec), now, previous);
}

void AuthService::logout(Session&) {
    // Nothing is kept on the server side; the caller discards the Session object.
}

void AuthService::changePassword(Session& s, const std::string& current, const std::string& newPw) {
    auto rec = r_.users.findById(s.getUser().getId());
    if (!rec) throw BusinessRuleException("Account not found.");
    if (!hasher_.verify(current, rec->passwordHash))
        throw BusinessRuleException("The current password is not correct.");
    validateNewPassword(newPw);
    if (newPw == current) throw ValidationException("The new password must be different from the current one.");
    rec->passwordHash = hasher_.hash(newPw);
    rec->mustChangePassword = false;
    r_.users.update(*rec);
    s.user().clearMustChangePassword();
}

// ===== UserService ==========================================================
void UserService::checkDepartmentRole(const UserRecord& rec, std::optional<int> dept) const {
    const Role role = roleFromString(rec.role);
    if (role == Role::LECTURER || role == Role::HEAD_OF_DEPARTMENT) {
        if (!dept || !r_.departments.findById(*dept))
            throw ValidationException("A lecturer or head of department needs an existing department.");
        if (role == Role::HEAD_OF_DEPARTMENT && rec.isActive) {
            auto hod = r_.users.findActiveHod(*dept);
            if (hod && hod->id != rec.id)
                throw BusinessRuleException("That department already has an active head: " + hod->fullName + ".");
        }
    }
}

CreatedUser UserService::createUser(const Session& s, const NewUser& data) {
    p_.require(s.getUser(), Function::MANAGE_ACCOUNTS);
    UserRecord rec;
    rec.username = trim(data.username);
    rec.fullName = trim(data.fullName);
    requireUsername(rec.username);
    requireLength("Full name", rec.fullName, 2, 100);
    if (r_.users.findByUsername(rec.username)) throw BusinessRuleException("That username is already taken.");
    rec.role = toString(data.role);
    rec.isActive = true;
    rec.mustChangePassword = true;

    if (data.role == Role::STUDENT) {
        std::string number = trim(data.studentNumber.value_or(""));
        requireLength("Student number", number, 3, 20);
        if (r_.users.findByStudentNumber(number)) throw BusinessRuleException("That student number is already in use.");
        rec.studentNumber = number;
    } else if (data.role != Role::ADMINISTRATOR) {
        rec.departmentId = data.departmentId;
        checkDepartmentRole(rec, data.departmentId);
    }

    CreatedUser out;
    out.temporaryPassword = hasher_.generateTemporaryPassword();
    rec.passwordHash = hasher_.hash(out.temporaryPassword);
    out.id = r_.users.insert(rec);
    return out;
}

void UserService::editUser(const Session& s, int userId, const std::string& fullName,
                           std::optional<int> departmentId) {
    p_.require(s.getUser(), Function::MANAGE_ACCOUNTS);
    auto rec = r_.users.findById(userId);
    if (!rec) throw ValidationException("That account does not exist.");
    rec->fullName = trim(fullName);
    requireLength("Full name", rec->fullName, 2, 100);
    const Role role = roleFromString(rec->role);
    if (role == Role::LECTURER || role == Role::HEAD_OF_DEPARTMENT) {
        const std::optional<int> dept = departmentId ? departmentId : rec->departmentId;
        checkDepartmentRole(*rec, dept);
        rec->departmentId = dept;
    }
    r_.users.update(*rec);
}

void UserService::setActive(const Session& s, int userId, bool active) {
    p_.require(s.getUser(), Function::MANAGE_ACCOUNTS);
    auto rec = r_.users.findById(userId);
    if (!rec) throw ValidationException("That account does not exist.");
    if (!active && userId == s.getUser().getId())
        throw BusinessRuleException("You cannot deactivate your own account.");
    rec->isActive = active;
    if (active) checkDepartmentRole(*rec, rec->departmentId);
    r_.users.update(*rec);
}

std::string UserService::resetPassword(const Session& s, int userId) {
    p_.require(s.getUser(), Function::MANAGE_ACCOUNTS);
    auto rec = r_.users.findById(userId);
    if (!rec) throw ValidationException("That account does not exist.");
    const std::string temp = hasher_.generateTemporaryPassword();
    rec->passwordHash = hasher_.hash(temp);
    rec->mustChangePassword = true;
    rec->failedAttempts = 0;
    rec->lockedUntil.reset();
    r_.users.update(*rec);
    return temp;
}

std::vector<UserRecord> UserService::listUsers(const Session& s) {
    p_.require(s.getUser(), Function::MANAGE_ACCOUNTS);
    return r_.users.listAll();
}

CreatedUser UserService::createInitialAdministrator() {
    if (!r_.users.listAll().empty())
        throw BusinessRuleException("Accounts already exist.");
    UserRecord rec;
    rec.username = "admin";
    rec.fullName = "System Administrator";
    rec.role = toString(Role::ADMINISTRATOR);
    CreatedUser out;
    out.temporaryPassword = hasher_.generateTemporaryPassword();
    rec.passwordHash = hasher_.hash(out.temporaryPassword);
    out.id = r_.users.insert(rec);
    return out;
}

} // namespace lms
