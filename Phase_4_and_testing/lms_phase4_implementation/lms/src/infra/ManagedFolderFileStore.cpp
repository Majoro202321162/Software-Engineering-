#include "infra/Infrastructure.h"
#include "common/Exceptions.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace lms {
namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool allowedExtension(const std::string& ext) {
    static const char* ok[] = {".pdf", ".docx", ".txt", ".zip", ".png", ".jpg"};
    for (const char* e : ok) if (ext == e) return true;
    return false;
}

} // namespace

ManagedFolderFileStore::ManagedFolderFileStore(std::string rootFolder)
    : root_(std::move(rootFolder)) {}

// FR-16: exists, not empty, 10 MB or less, extension PDF, DOCX, TXT, ZIP, PNG or JPG.
void ManagedFolderFileStore::validate(const std::string& path) {
    if (path.empty()) throw FileException("No file name was entered.");
    std::error_code ec;
    const fs::path p(path);
    if (!fs::exists(p, ec) || ec) throw FileException("The file does not exist: " + path);
    if (!fs::is_regular_file(p, ec) || ec) throw FileException("That path is not a file.");
    const auto size = fs::file_size(p, ec);
    if (ec) throw FileException("The file cannot be read.");
    if (size == 0) throw FileException("The file is empty.");
    if (static_cast<long long>(size) > kMaxBytes)
        throw FileException("The file is larger than 10 MB.");
    if (!allowedExtension(lower(p.extension().string())))
        throw FileException("The file type is not allowed. Use PDF, DOCX, TXT, ZIP, PNG or JPG.");
    std::ifstream probe(p, std::ios::binary);
    if (!probe) throw FileException("The file cannot be opened for reading.");
}

// The destination is built by the system; a path typed by the user is only read (FR-16).
StoredFile ManagedFolderFileStore::store(const std::string& path, int assignmentId,
                                         int studentId, int versionNo) {
    validate(path);
    const fs::path src(path);
    const std::string ext = lower(src.extension().string());
    const std::string name = "a" + std::to_string(assignmentId) + "_s" +
                             std::to_string(studentId) + "_v" + std::to_string(versionNo) + ext;
    std::error_code ec;
    fs::create_directories(root_, ec);
    if (ec) throw FileException("The submissions folder cannot be created.");
    const fs::path dest = fs::path(root_) / name;
    fs::copy_file(src, dest, fs::copy_options::overwrite_existing, ec);
    if (ec) throw FileException("The file could not be saved (is the disk full?).");
    StoredFile out;
    out.storedPath = dest.string();
    out.originalName = src.filename().string();
    out.sizeBytes = static_cast<long long>(fs::file_size(dest, ec));
    return out;
}

void ManagedFolderFileStore::remove(const std::string& storedPath) noexcept {
    std::error_code ec;
    fs::remove(fs::path(storedPath), ec);
}

std::string ManagedFolderFileStore::open(const std::string& storedPath) {
    std::error_code ec;
    if (!fs::exists(storedPath, ec) || ec) throw FileException("The stored file is missing.");
    return fs::absolute(storedPath, ec).string();
}

std::string ManagedFolderFileStore::readTextPreview(const std::string& storedPath, int maxLines) {
    if (lower(fs::path(storedPath).extension().string()) != ".txt")
        return "(preview is available for .txt files only)";
    std::ifstream in(storedPath);
    if (!in) throw FileException("The stored file cannot be read.");
    std::string line, out;
    for (int i = 0; i < maxLines && std::getline(in, line); ++i) out += line + "\n";
    return out;
}

void ManagedFolderFileStore::copyStoreTo(const std::string& folder) {
    std::error_code ec;
    fs::create_directories(folder, ec);
    if (ec) throw FileException("The backup folder cannot be created.");
    if (!fs::exists(root_, ec)) return;                       // nothing submitted yet
    fs::copy(root_, fs::path(folder) / "submissions",
             fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) throw FileException("The submissions folder could not be copied.");
}

} // namespace lms
