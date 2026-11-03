// Composition root: the only place that names the concrete SQLite and libsodium classes
// (Dependency inversion, Phase 3 Table 3.4).
#include "business/Services.h"
#include "data/SqliteRepositories.h"
#include "presentation/ConsoleApp.h"

#include <cstring>
#include <iostream>

using namespace lms;

namespace {
void usage() {
    std::cout << "Moderated LMS\n"
                 "Usage: lms [--db FILE] [--store FOLDER]\n"
                 "  --db FILE        SQLite database file (default: lms.db)\n"
                 "  --store FOLDER   folder for submitted files (default: submissions)\n";
}
}

int main(int argc, char** argv) {
    std::string dbPath = "lms.db", storePath = "submissions";
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--db") && i + 1 < argc) dbPath = argv[++i];
        else if (!std::strcmp(argv[i], "--store") && i + 1 < argc) storePath = argv[++i];
        else { usage(); return std::strcmp(argv[i], "--help") == 0 ? 0 : 2; }
    }

    try {
        Database db;
        db.open(dbPath);

        SqliteUserRepository users(db);
        SqliteDepartmentRepository departments(db);
        SqliteCourseRepository courses(db);
        SqliteAssignmentRepository assignments(db);
        SqliteExtensionRepository extensions(db);
        SqliteSubmissionRepository submissions(db);
        SqliteMarkRepository marks(db);
        SqliteAuditRepository audit(db);
        SqliteEventLogRepository events(db);
        SqliteUnitOfWork uow(db);
        RepoSet repos{users, departments, courses, assignments, extensions,
                      submissions, marks, audit, events, uow};

        SystemClock clock;
        LibsodiumArgon2idHasher hasher;
        ManagedFolderFileStore files(storePath);
        TenPercentPerDayPenalty penalty;
        PermissionPolicy policy(events, clock);

        AuthService auth(repos, hasher, clock);
        UserService userService(repos, hasher, policy, clock);
        CourseService courseService(repos, policy, clock);
        AssignmentService assignmentService(repos, policy, clock);
        AuditService auditService(repos, policy, clock);
        SubmissionService submissionService(repos, policy, clock, files, penalty, assignmentService);
        MarkingService markingService(repos, policy, clock, penalty, assignmentService, auditService);
        ModerationService moderationService(repos, policy, clock, penalty, auditService);
        BackupService backupService(uow, files, policy, clock);
        ReportExporter exporter(repos, policy);

        // First start: create the first administrator and show its temporary password once.
        if (users.listAll().empty()) {
            CreatedUser first = userService.createInitialAdministrator();
            std::cout << "\nFIRST START: administrator account created.\n"
                      << "  Username: admin\n  Temporary password: " << first.temporaryPassword
                      << "\n  You must change it at the first login. It is not shown again.\n";
        }

        InputReader reader(std::cin, std::cout);
        UiContext ui{auth, userService, courseService, assignmentService, submissionService,
                     markingService, moderationService, auditService, backupService, exporter,
                     clock, reader, std::cout};
        return ConsoleApp(ui).run();
    } catch (const LmsException& e) {
        std::cerr << "Cannot start: " << e.userMessage() << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Cannot start: " << e.what() << "\n";
    }
    return 1;
}
