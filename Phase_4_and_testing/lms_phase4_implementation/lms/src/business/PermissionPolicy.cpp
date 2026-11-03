#include "business/PermissionPolicy.h"
#include "common/Exceptions.h"

namespace lms {

bool PermissionPolicy::isAllowed(const User& u, Function f) const {
    return u.isActive() && u.permittedFunctions().count(f) > 0;
}

void PermissionPolicy::require(const User& u, Function f) const {
    if (isAllowed(u, f)) return;
    deny(u, "Function " + std::to_string(static_cast<int>(f)) + " refused for role " +
                toString(u.getRole()));
}

void PermissionPolicy::deny(const User& u, const std::string& detail) const {
    try {
        log_.append(u.getId(), EventType::ACCESS_DENIED, detail, clock_.now());
    } catch (const std::exception&) {
        // Logging must never hide the refusal itself.
    }
    throw AuthorizationException();
}

} // namespace lms
