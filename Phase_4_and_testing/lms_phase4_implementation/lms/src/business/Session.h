// Logged-in user plus the time of the last input (Phase 3, Figure 3.3, FR-05).
#pragma once
#include "domain/User.h"

#include <chrono>
#include <memory>

namespace lms {

class Session {
public:
    static constexpr std::chrono::minutes kTimeout{10};

    Session(std::unique_ptr<User> user, TimePoint now, std::optional<TimePoint> previousLogin)
        : user_(std::move(user)), lastActivity_(now), previousLogin_(previousLogin) {}

    void touch(TimePoint now) { lastActivity_ = now; }
    bool isExpired(TimePoint now) const { return now - lastActivity_ > kTimeout; }
    const User& getUser() const { return *user_; }
    User& user() { return *user_; }
    // Time of the login before this one (used for FR-32).
    std::optional<TimePoint> previousLogin() const { return previousLogin_; }

private:
    std::unique_ptr<User> user_;
    TimePoint lastActivity_;
    std::optional<TimePoint> previousLogin_;
};

} // namespace lms
