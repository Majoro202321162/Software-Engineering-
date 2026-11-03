#include "business/Services.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace lms {
namespace {
// CSV cell: quotes doubled; a leading = + - @ is neutralised so spreadsheets do not run it.
std::string csvCell(std::string v) {
    if (!v.empty() && (v[0] == '=' || v[0] == '+' || v[0] == '-' || v[0] == '@')) v = "'" + v;
    std::string out = "\"";
    for (char ch : v) { if (ch == '"') out += '"'; out += ch; }
    return out + "\"";
}
} // namespace

// FR-34: one new folder per backup: lms.db (consistent copy) and submissions/.
std::string BackupService::backup(const Session& s, const std::string& folder) {
    p_.require(s.getUser(), Function::BACK_UP_DATA);
    if (trim(folder).empty()) throw ValidationException("Enter a folder for the backup.");
    std::string stamp = toIso(clock_.now());
    for (char& ch : stamp) if (ch == ':' || ch == '-') ch = '_';
    const fs::path target = fs::path(trim(folder)) / ("lms_backup_" + stamp);
    std::error_code ec;
    fs::create_directories(target, ec);
    if (ec) throw FileException("The backup folder cannot be created.");
    db_.backupDatabase((target / "lms.db").string());
    files_.copyStoreTo(target.string());
    return target.string();
}

// FR-33: released marks of one assignment as CSV.
int ReportExporter::exportReleasedMarks(const Session& s, int assignmentId, const std::string& csvPath) {
    p_.require(s.getUser(), Function::EXPORT_MARKS);
    Assignment a = scope_.assignment(assignmentId);
    scope_.staffCourse(s, a.courseId);
    RowFilter f;
    f.assignmentId = assignmentId;
    f.status = SubmissionStatus::RELEASED;
    auto rows = r_.submissions.listRows(f);
    if (trim(csvPath).empty()) throw ValidationException("Enter a file name for the export.");
    std::ofstream out(trim(csvPath));
    if (!out) throw FileException("The export file cannot be written.");
    out << "student_number,student_name,raw_mark,days_late,penalty,final_mark,max_mark\n";
    for (const auto& r : rows) {
        if (!r.mark) continue;
        out << csvCell(r.studentNumber) << ',' << csvCell(r.studentName) << ',' << formatMark(r.mark->rawMark)
            << ',' << r.mark->daysLate << ',' << formatMark(r.mark->penalty) << ','
            << formatMark(r.mark->finalMark) << ',' << r.maxMark << '\n';
    }
    if (!out) throw FileException("The export file could not be written completely.");
    return static_cast<int>(rows.size());
}

} // namespace lms
