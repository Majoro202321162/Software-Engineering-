// Second, independent authorisation check (the menu is the first). Every service method
// calls require(); a refused call is written to event_log (Table 2.4, FR-03, NFR-07).
#pragma once
#include "business/Session.h"
#include "data/Repositories.h"
#include "infra/Infrastructure.h"

namespace lms {

class PermissionPolicy {
public:
    PermissionPolicy(IEventLogRepository& log, IClock& clock) : log_(log), clock_(clock) {}

    bool isAllowed(const User& u, Function f) const;
    void require(const User& u, Function f) const;                     // throws AuthorizationException
    // Scope violation (another lecturer's course, another department, ...).
    [[noreturn]] void deny(const User& u, const std::string& detail) const;

private:
    IEventLogRepository& log_;
    IClock& clock_;
};

} // namespace lms
