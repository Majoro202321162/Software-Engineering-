// Infrastructure interfaces: clock, password hashing, managed file store
// (Phase 3, Figure 3.6). The business layer depends only on these interfaces.
#pragma once
#include "common/TimeUtil.h"

#include <memory>
#include <string>

namespace lms {

class IClock {
public:
    virtual ~IClock() = default;
    virtual TimePoint now() const = 0;           // the only source of time (DD-08)
};

class SystemClock : public IClock {
public:
    TimePoint now() const override { return std::chrono::system_clock::now(); }
};

// Lets tests set the time to the second.
class FixedClock : public IClock {
public:
    explicit FixedClock(TimePoint t) : t_(t) {}
    TimePoint now() const override { return t_; }
    void set(TimePoint t) { t_ = t; }
    void advance(std::chrono::seconds s) { t_ += s; }
private:
    TimePoint t_;
};

class IPasswordHasher {
public:
    virtual ~IPasswordHasher() = default;
    virtual std::string hash(const std::string& password) = 0;
    virtual bool verify(const std::string& password, const std::string& hash) = 0;
    // Random temporary password: at least 10 characters, a letter and a digit.
    virtual std::string generateTemporaryPassword() = 0;
};

// Argon2id through libsodium (NFR-05). Implemented in LibsodiumHasher.cpp,
// the only file that includes <sodium.h>.
class LibsodiumArgon2idHasher : public IPasswordHasher {
public:
    LibsodiumArgon2idHasher();
    std::string hash(const std::string& password) override;
    bool verify(const std::string& password, const std::string& hash) override;
    std::string generateTemporaryPassword() override;
};

struct StoredFile {
    std::string storedPath;     // path of the managed copy (built by the system)
    std::string originalName;   // original file name, kept as text only
    long long sizeBytes = 0;
};

class IFileStore {
public:
    virtual ~IFileStore() = default;
    virtual void validate(const std::string& path) = 0;     // throws FileException
    virtual StoredFile store(const std::string& path, int assignmentId, int studentId,
                             int versionNo) = 0;
    virtual void remove(const std::string& storedPath) noexcept = 0;   // undo after a failed save
    virtual std::string open(const std::string& storedPath) = 0;       // absolute path, checked
    virtual std::string readTextPreview(const std::string& storedPath, int maxLines) = 0;
    virtual void copyStoreTo(const std::string& folder) = 0;           // for backups (FR-34)
};

class ManagedFolderFileStore : public IFileStore {
public:
    explicit ManagedFolderFileStore(std::string rootFolder);
    void validate(const std::string& path) override;
    StoredFile store(const std::string& path, int assignmentId, int studentId,
                     int versionNo) override;
    void remove(const std::string& storedPath) noexcept override;
    std::string open(const std::string& storedPath) override;
    std::string readTextPreview(const std::string& storedPath, int maxLines) override;
    void copyStoreTo(const std::string& folder) override;
    static constexpr long long kMaxBytes = 10LL * 1024 * 1024;
private:
    std::string root_;
};

} // namespace lms
