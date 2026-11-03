// Exception hierarchy (Phase 3, Figure 3.7 / Table 3.2).
#pragma once
#include <stdexcept>
#include <string>

namespace lms {

class LmsException : public std::runtime_error {
public:
    explicit LmsException(const std::string& message) : std::runtime_error(message) {}
    virtual std::string userMessage() const { return what(); }
};

class ValidationException : public LmsException {
public:
    using LmsException::LmsException;
};

class AuthorizationException : public LmsException {
public:
    AuthorizationException() : LmsException("You are not allowed to do this.") {}
};

class BusinessRuleException : public LmsException {
public:
    using LmsException::LmsException;
};

// Raised when the 10-minute inactivity limit is passed (FR-05).
class SessionExpiredException : public BusinessRuleException {
public:
    SessionExpiredException()
        : BusinessRuleException("Your session ended after 10 minutes without input.") {}
};

class DataAccessException : public LmsException {
public:
    explicit DataAccessException(const std::string& technical)
        : LmsException(technical) {}
    std::string userMessage() const override {
        return "A database error occurred. Nothing was changed.";
    }
};

class FileException : public LmsException {
public:
    using LmsException::LmsException;
};

// Input stream closed (Ctrl-D / end of redirected input). Not an LmsException:
// it ends the program normally instead of being shown as an error.
class EndOfInput : public std::runtime_error {
public:
    EndOfInput() : std::runtime_error("end of input") {}
};

} // namespace lms
