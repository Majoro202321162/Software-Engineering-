// The command catalogue: one ICommand per menu action for all four roles (DD-05).
// Each command validates its input with InputReader, asks for confirmation before final
// actions and then calls exactly one service method. It contains no business rules and no SQL.
#include "presentation/Menu.h"

#include <iomanip>
#include <ostream>

namespace lms {
namespace {

using Action = ActionCommand::Action;

void add(CommandGroup& g, const std::string& label, Function f, Action a) {
    g.commands.push_back(std::make_unique<ActionCommand>(label, f, std::move(a)));
}

std::string fit(std::string s, size_t w) {
    if (s.size() > w) s = s.substr(0, w - 1) + "~";
    return s + std::string(w - s.size(), ' ');
}

std::string yesNo(bool b) { return b ? "yes" : "no"; }

// ---- pickers ----------------------------------------------------------------
std::optional<Course> pickCourse(Session& s, UiContext& ui) {
    auto courses = ui.courses.coursesFor(s);
    if (courses.empty()) { ui.out << "  You have no courses.\n"; return std::nullopt; }
    ui.out << "\n  " << fit("ID", 5) << fit("Code", 12) << "Title\n";
    for (const Course& c : courses)
        ui.out << "  " << fit(std::to_string(c.id), 5) << fit(c.code, 12) << c.title << (c.isActive ? "" : "  (closed)") << "\n";
    auto id = ui.in.readId("Course id");
    if (!id) return std::nullopt;
    for (const Course& c : courses) if (c.id == *id) return c;
    ui.out << "  That course is not in the list.\n";
    return std::nullopt;
}

std::optional<Assignment> pickAssignmentOf(Session& s, UiContext& ui, int courseId) {
    auto list = ui.assignments.listByCourse(s, courseId);
    if (list.empty()) { ui.out << "  This course has no assignments.\n"; return std::nullopt; }
    ui.out << "\n  " << fit("ID", 5) << fit("Title", 30) << fit("Deadline", 18) << fit("Max", 5) << "State\n";
    for (const Assignment& a : list)
        ui.out << "  " << fit(std::to_string(a.id), 5) << fit(a.title, 30) << fit(toLocalDisplay(a.deadline), 18)
               << fit(std::to_string(a.maxMark), 5) << (a.isClosed ? "closed" : "open") << "\n";
    auto id = ui.in.readId("Assignment id");
    if (!id) return std::nullopt;
    for (const Assignment& a : list) if (a.id == *id) return a;
    ui.out << "  That assignment is not in the list.\n";
    return std::nullopt;
}

std::optional<Assignment> pickAssignment(Session& s, UiContext& ui) {
    auto c = pickCourse(s, ui);
    if (!c) return std::nullopt;
    return pickAssignmentOf(s, ui, c->id);
}

std::optional<UserRecord> pickUser(Session& s, UiContext& ui, std::optional<Role> role, const std::string& what) {
    std::vector<UserRecord> list;
    for (const UserRecord& u : ui.users.listUsers(s))
        if (!role || u.role == toString(*role)) list.push_back(u);
    if (list.empty()) { ui.out << "  No matching accounts.\n"; return std::nullopt; }
    ui.out << "\n  " << fit("ID", 5) << fit("Username", 16) << fit("Name", 26) << fit("Role", 20) << "Active\n";
    for (const UserRecord& u : list)
        ui.out << "  " << fit(std::to_string(u.id), 5) << fit(u.username, 16) << fit(u.fullName, 26)
               << fit(u.role, 20) << yesNo(u.isActive) << "\n";
    auto id = ui.in.readId(what);
    if (!id) return std::nullopt;
    for (const UserRecord& u : list) if (u.id == *id) return u;
    ui.out << "  That account is not in the list.\n";
    return std::nullopt;
}

std::optional<Department> pickDepartment(Session& s, UiContext& ui) {
    auto list = ui.courses.listDepartments(s);
    if (list.empty()) { ui.out << "  There are no departments yet.\n"; return std::nullopt; }
    ui.out << "\n  " << fit("ID", 5) << "Name\n";
    for (const Department& d : list) ui.out << "  " << fit(std::to_string(d.id), 5) << d.name << "\n";
    auto id = ui.in.readId("Department id");
    if (!id) return std::nullopt;
    for (const Department& d : list) if (d.id == *id) return d;
    ui.out << "  That department is not in the list.\n";
    return std::nullopt;
}

std::optional<Course> pickAnyCourse(Session& s, UiContext& ui) {
    auto list = ui.courses.listAllCourses(s);
    if (list.empty()) { ui.out << "  There are no courses yet.\n"; return std::nullopt; }
    ui.out << "\n  " << fit("ID", 5) << fit("Code", 12) << fit("Title", 30) << "State\n";
    for (const Course& c : list)
        ui.out << "  " << fit(std::to_string(c.id), 5) << fit(c.code, 12) << fit(c.title, 30) << (c.isActive ? "open" : "closed") << "\n";
    auto id = ui.in.readId("Course id");
    if (!id) return std::nullopt;
    for (const Course& c : list) if (c.id == *id) return c;
    ui.out << "  That course is not in the list.\n";
    return std::nullopt;
}

// ---- printing ---------------------------------------------------------------
void printRows(UiContext& ui, const std::vector<SubmissionRow>& rows, bool withMarks) {
    if (rows.empty()) { ui.out << "  No submissions match.\n"; return; }
    ui.out << "\n  " << fit("ID", 5) << fit("Student", 22) << fit("Status", 11) << fit("Handed in", 17)
           << fit("Late", 5);
    if (withMarks) ui.out << fit("Raw", 6) << fit("Pen.", 6) << fit("Final", 6) << "Marker";
    ui.out << "\n";
    for (const SubmissionRow& r : rows) {
        ui.out << "  " << fit(std::to_string(r.submissionId), 5) << fit(r.studentName + " " + r.studentNumber, 22)
               << fit(staffStatusText(r.status), 11) << fit(toLocalDisplay(r.uploadedAt), 17)
               << fit(std::to_string(r.daysLate), 5);
        if (withMarks) {
            if (r.mark)
                ui.out << fit(formatMark(r.mark->rawMark), 6) << fit(formatMark(r.mark->penalty), 6)
                       << fit(formatMark(r.mark->finalMark), 6) << r.markedByName;
            else ui.out << fit("-", 6) << fit("-", 6) << fit("-", 6);
        }
        ui.out << "\n";
    }
}

std::optional<SubmissionRow> pickRow(UiContext& ui, const std::vector<SubmissionRow>& rows,
                                     const std::string& what) {
    auto id = ui.in.readId(what);
    if (!id) return std::nullopt;
    for (const SubmissionRow& r : rows) if (r.submissionId == *id) return r;
    ui.out << "  That submission is not in the list.\n";
    return std::nullopt;
}

// ---- command groups ---------------------------------------------------------
CommandGroup accountGroup() {
    CommandGroup g{"My account", {}};
    add(g, "Change my password", Function::CHANGE_PASSWORD, [](Session& s, UiContext& ui) {
        const std::string cur = ui.in.readPassword("Current password: ");
        const std::string n1 = ui.in.readPassword("New password (8+ characters, a letter and a digit): ");
        const std::string n2 = ui.in.readPassword("Repeat new password: ");
        if (n1 != n2) { ui.out << "  The two passwords are different. Nothing was changed.\n"; return; }
        ui.auth.changePassword(s, cur, n1);
        ui.out << "  Password changed.\n";
    });
    return g;
}

CommandGroup accountsAdminGroup() {
    CommandGroup g{"Manage user accounts", {}};
    add(g, "List accounts", Function::MANAGE_ACCOUNTS, [](Session& s, UiContext& ui) {
        pickUser(s, ui, std::nullopt, "Press 0 to go back; account id");
    });
    add(g, "Create an account", Function::MANAGE_ACCOUNTS, [](Session& s, UiContext& ui) {
        NewUser n;
        ui.out << "  Roles: 1 Administrator, 2 Lecturer, 3 Head of Department, 4 Student\n";
        const int r = ui.in.readInt("Role: ", 1, 4);
        n.role = r == 1 ? Role::ADMINISTRATOR : r == 2 ? Role::LECTURER : r == 3 ? Role::HEAD_OF_DEPARTMENT : Role::STUDENT;
        n.username = ui.in.readText("Username (3-30 characters): ", 3, 30);
        n.fullName = ui.in.readText("Full name: ", 2, 100);
        if (n.role == Role::LECTURER || n.role == Role::HEAD_OF_DEPARTMENT) {
            auto d = pickDepartment(s, ui);
            if (!d) return;
            n.departmentId = d->id;
        } else if (n.role == Role::STUDENT) {
            n.studentNumber = ui.in.readText("Student number: ", 3, 20);
        }
        CreatedUser c = ui.users.createUser(s, n);
        ui.out << "  Account created (id " << c.id << ").\n  Temporary password: " << c.temporaryPassword
               << "\n  Give it to the user; it must be changed at the first login. It is not shown again.\n";
    });
    add(g, "Edit an account (name or department)", Function::MANAGE_ACCOUNTS, [](Session& s, UiContext& ui) {
        auto u = pickUser(s, ui, std::nullopt, "Account id");
        if (!u) return;
        const std::string name = ui.in.readText("New full name: ", 2, 100);
        std::optional<int> dept;
        if (u->role == toString(Role::LECTURER) || u->role == toString(Role::HEAD_OF_DEPARTMENT)) {
            auto d = pickDepartment(s, ui);
            if (!d) return;
            dept = d->id;
        }
        ui.users.editUser(s, u->id, name, dept);
        ui.out << "  Account updated.\n";
    });
    add(g, "Deactivate or reactivate an account", Function::MANAGE_ACCOUNTS, [](Session& s, UiContext& ui) {
        auto u = pickUser(s, ui, std::nullopt, "Account id");
        if (!u) return;
        const bool activate = !u->isActive;
        if (!ui.in.confirm(std::string(activate ? "Reactivate " : "Deactivate ") + u->username + "?")) return;
        ui.users.setActive(s, u->id, activate);
        ui.out << "  Done. Accounts are never deleted.\n";
    });
    add(g, "Reset a password", Function::MANAGE_ACCOUNTS, [](Session& s, UiContext& ui) {
        auto u = pickUser(s, ui, std::nullopt, "Account id");
        if (!u || !ui.in.confirm("Reset the password of " + u->username + "?")) return;
        ui.out << "  Temporary password: " << ui.users.resetPassword(s, u->id)
               << "\n  It must be changed at the next login. It is not shown again.\n";
    });
    return g;
}

CommandGroup structureGroup() {
    CommandGroup g{"Departments, courses and enrolment", {}};
    add(g, "Create a department", Function::MANAGE_STRUCTURE, [](Session& s, UiContext& ui) {
        const std::string name = ui.in.readText("Department name: ", 2, 100);
        ui.out << "  Department created (id " << ui.courses.createDepartment(s, name) << ").\n";
    });
    add(g, "Rename a department", Function::MANAGE_STRUCTURE, [](Session& s, UiContext& ui) {
        auto d = pickDepartment(s, ui);
        if (!d) return;
        ui.courses.renameDepartment(s, d->id, ui.in.readText("New name: ", 2, 100));
        ui.out << "  Department renamed.\n";
    });
    add(g, "Create a course", Function::MANAGE_STRUCTURE, [](Session& s, UiContext& ui) {
        auto d = pickDepartment(s, ui);
        if (!d) return;
        const std::string code = ui.in.readText("Course code (2-20 characters): ", 2, 20);
        const std::string title = ui.in.readText("Course title: ", 2, 100);
        ui.out << "  Course created (id " << ui.courses.createCourse(s, code, title, d->id) << ").\n";
    });
    add(g, "List courses", Function::MANAGE_STRUCTURE, [](Session& s, UiContext& ui) { pickAnyCourse(s, ui); });
    add(g, "Edit or close a course", Function::MANAGE_STRUCTURE, [](Session& s, UiContext& ui) {
        auto c = pickAnyCourse(s, ui);
        if (!c) return;
        const std::string title = ui.in.readText("Course title: ", 2, 100);
        const bool open = ui.in.confirm("Keep the course open");
        ui.courses.editCourse(s, c->id, title, open);
        ui.out << "  Course updated.\n";
    });
    add(g, "Assign a lecturer to a course", Function::MANAGE_STRUCTURE, [](Session& s, UiContext& ui) {
        auto c = pickAnyCourse(s, ui);
        if (!c) return;
        auto u = pickUser(s, ui, Role::LECTURER, "Lecturer id");
        if (!u) return;
        ui.courses.assignLecturer(s, c->id, u->id);
        ui.out << "  Lecturer assigned.\n";
    });
    add(g, "Remove a lecturer from a course", Function::MANAGE_STRUCTURE, [](Session& s, UiContext& ui) {
        auto c = pickAnyCourse(s, ui);
        if (!c) return;
        ui.out << "  Lecturers of " << c->code << ":\n";
        for (const UserRecord& u : ui.courses.lecturersOf(s, c->id))
            ui.out << "    " << u.id << "  " << u.fullName << "\n";
        auto id = ui.in.readId("Lecturer id");
        if (!id) return;
        ui.courses.removeLecturer(s, c->id, *id);
        ui.out << "  Lecturer removed.\n";
    });
    add(g, "Enrol a student", Function::MANAGE_STRUCTURE, [](Session& s, UiContext& ui) {
        auto c = pickAnyCourse(s, ui);
        if (!c) return;
        auto u = pickUser(s, ui, Role::STUDENT, "Student id");
        if (!u) return;
        ui.courses.enrol(s, c->id, u->id);
        ui.out << "  Student enrolled.\n";
    });
    add(g, "Unenrol a student (history is kept)", Function::MANAGE_STRUCTURE, [](Session& s, UiContext& ui) {
        auto c = pickAnyCourse(s, ui);
        if (!c) return;
        ui.out << "  Enrolled students of " << c->code << ":\n";
        for (const UserRecord& u : ui.courses.studentsOf(s, c->id))
            ui.out << "    " << u.id << "  " << u.fullName << "  " << u.studentNumber.value_or("") << "\n";
        auto id = ui.in.readId("Student id");
        if (!id || !ui.in.confirm("Unenrol this student? Submissions and marks are kept")) return;
        ui.courses.unenrol(s, c->id, *id);
        ui.out << "  Student unenrolled.\n";
    });
    return g;
}

CommandGroup backupGroup() {
    CommandGroup g{"Back up data", {}};
    add(g, "Back up the database and submitted files", Function::BACK_UP_DATA, [](Session& s, UiContext& ui) {
        const std::string folder = ui.in.readText("Backup folder: ", 1, 260);
        if (!ui.in.confirm("Create a backup in '" + folder + "'")) return;
        ui.out << "  Backup written to: " << ui.backup.backup(s, folder) << "\n"
               << "  To restore: stop the program and copy lms.db and the submissions folder back.\n";
    });
    return g;
}

CommandGroup assignmentGroup() {
    CommandGroup g{"Assignments and extensions", {}};
    add(g, "Create an assignment", Function::MANAGE_ASSIGNMENTS, [](Session& s, UiContext& ui) {
        auto c = pickCourse(s, ui);
        if (!c) return;
        NewAssignment n;
        n.courseId = c->id;
        n.title = ui.in.readText("Title: ", 2, 100);
        n.description = ui.in.readOptionalText("Description (optional): ", 2000);
        n.deadline = ui.in.readDateTime("Deadline");
        n.maxMark = ui.in.readInt("Maximum mark (1-100): ", 1, 100);
        ui.out << "  Assignment created (id " << ui.assignments.createAssignment(s, n) << ").\n";
    });
    add(g, "List my assignments", Function::MANAGE_ASSIGNMENTS, [](Session& s, UiContext& ui) {
        auto c = pickCourse(s, ui);
        if (c) pickAssignmentOf(s, ui, c->id);
    });
    add(g, "Edit or close an assignment", Function::MANAGE_ASSIGNMENTS, [](Session& s, UiContext& ui) {
        auto a = pickAssignment(s, ui);
        if (!a) return;
        ui.out << "  1 Change title/description, 2 Move deadline later, 3 Close, 4 Reopen\n";
        AssignmentEdit e;
        switch (ui.in.readInt("Action: ", 1, 4)) {
            case 1:
                e.title = ui.in.readText("Title: ", 2, 100);
                e.description = ui.in.readOptionalText("Description: ", 2000);
                break;
            case 2: e.deadline = ui.in.readDateTime("New deadline"); break;
            case 3: if (!ui.in.confirm("Close it to new submissions")) return; e.close = true; break;
            default: e.close = false; break;
        }
        ui.assignments.editAssignment(s, a->id, e);
        ui.out << "  Assignment updated.\n";
    });
    add(g, "Grant an extension to one student", Function::GRANT_EXTENSION, [](Session& s, UiContext& ui) {
        auto c = pickCourse(s, ui);
        if (!c) return;
        auto a = pickAssignmentOf(s, ui, c->id);
        if (!a) return;
        ui.out << "\n  Enrolled students:\n";
        for (const UserRecord& u : ui.assignments.enrolledStudents(s, a->id))
            ui.out << "    " << u.id << "  " << u.fullName << "  " << u.studentNumber.value_or("") << "\n";
        auto id = ui.in.readId("Student id");
        if (!id) return;
        const TimePoint nd = ui.in.readDateTime("New deadline for this student");
        const std::string reason = ui.in.readText("Reason (at least 10 characters): ", 10, 500);
        if (!ui.in.confirm("Grant the extension")) return;
        ui.assignments.grantExtension(s, a->id, *id, nd, reason);
        ui.out << "  Extension granted. It is recorded with your name, the time and the reason.\n";
    });
    return g;
}

CommandGroup markingGroup() {
    CommandGroup g{"Submissions and marking", {}};
    add(g, "View submissions of an assignment", Function::VIEW_SUBMISSIONS, [](Session& s, UiContext& ui) {
        auto a = pickAssignment(s, ui);
        if (a) printRows(ui, ui.submissions.listForLecturer(s, a->id), true);
    });
    add(g, "Open a submitted file", Function::VIEW_SUBMISSIONS, [](Session& s, UiContext& ui) {
        auto a = pickAssignment(s, ui);
        if (!a) return;
        auto rows = ui.submissions.listForLecturer(s, a->id);
        printRows(ui, rows, false);
        auto r = pickRow(ui, rows, "Submission id");
        if (!r) return;
        OpenedFile f = ui.submissions.openFile(s, r->submissionId);
        ui.out << "  File: " << f.originalName << " (version " << f.versionNo << ", " << f.sizeBytes << " bytes)\n"
               << "  Stored at: " << f.path << "\n";
        if (!f.preview.empty()) ui.out << "  ---- preview ----\n" << f.preview << "  -----------------\n";
    });
    add(g, "Mark a submission", Function::MARK_SUBMISSION, [](Session& s, UiContext& ui) {
        auto a = pickAssignment(s, ui);
        if (!a) return;
        auto rows = ui.submissions.listForLecturer(s, a->id);
        printRows(ui, rows, true);
        auto r = pickRow(ui, rows, "Submission id to mark");
        if (!r) return;
        ui.out << "  Student: " << r->studentName << ", status " << staffStatusText(r->status)
               << ", days late " << r->daysLate << ", maximum mark " << r->maxMark << "\n";
        const double raw = ui.in.readMark("Raw mark (0-" + std::to_string(r->maxMark) + ", steps of 0.5): ", r->maxMark);
        const std::string fb = ui.in.readText("Written feedback (10-2000 characters): ", 10, 2000);
        if (!ui.in.confirm("Save this mark (the late penalty is applied automatically)")) return;
        MarkResult m = ui.marking.mark(s, r->submissionId, raw, fb);
        ui.out << "  Saved. Raw " << formatMark(m.rawMark) << ", days late " << m.daysLate << ", penalty "
               << formatMark(m.penalty) << ", final mark " << formatMark(m.finalMark)
               << ". Status: Marked (students see it only after the HoD releases it).\n";
    });
    add(g, "Re-mark a submission (before moderation)", Function::MARK_SUBMISSION, [](Session& s, UiContext& ui) {
        auto a = pickAssignment(s, ui);
        if (!a) return;
        auto rows = ui.submissions.listForLecturer(s, a->id);
        printRows(ui, rows, true);
        auto r = pickRow(ui, rows, "Submission id to re-mark");
        if (!r || !r->mark) { if (r) ui.out << "  That work has not been marked yet.\n"; return; }
        if (r->mark->hodComment) ui.out << "  HoD comment: " << *r->mark->hodComment << "\n";
        ui.out << "  Current feedback: " << r->mark->feedback << "\n";
        const double raw = ui.in.readMark("New raw mark (0-" + std::to_string(r->maxMark) + "): ", r->maxMark);
        std::string fb = ui.in.readOptionalText("New feedback (blank = keep current): ", 2000);
        if (fb.empty()) fb = r->mark->feedback;
        const std::string reason = ui.in.readText("Reason for the change (at least 10 characters): ", 10, 500);
        if (!ui.in.confirm("Save the changed mark")) return;
        MarkResult m = ui.marking.remark(s, r->submissionId, raw, fb, reason);
        ui.out << "  Saved. Final mark " << formatMark(m.finalMark) << ". The change is in the audit log.\n";
    });
    add(g, "Export released marks to a CSV file", Function::EXPORT_MARKS, [](Session& s, UiContext& ui) {
        auto a = pickAssignment(s, ui);
        if (!a) return;
        const std::string path = ui.in.readText("CSV file name: ", 1, 260);
        ui.out << "  " << ui.exporter.exportReleasedMarks(s, a->id, path) << " released mark(s) written.\n";
    });
    return g;
}

RowFilter askFilter(Session& s, UiContext& ui) {
    RowFilter f;
    if (auto c = pickCourse(s, ui)) {
        f.courseId = c->id;
        if (auto a = pickAssignmentOf(s, ui, c->id)) f.assignmentId = a->id;
    }
    ui.out << "  Status: 0 all, 1 Marked, 2 Returned, 3 Moderated, 4 Released\n";
    switch (ui.in.readInt("Status filter: ", 0, 4)) {
        case 1: f.status = SubmissionStatus::MARKED; break;
        case 2: f.status = SubmissionStatus::RETURNED; break;
        case 3: f.status = SubmissionStatus::MODERATED; break;
        case 4: f.status = SubmissionStatus::RELEASED; break;
        default: break;
    }
    return f;
}

CommandGroup moderationGroup() {
    CommandGroup g{"Moderation and release", {}};
    add(g, "Review marks of my department", Function::MODERATE_MARKS, [](Session& s, UiContext& ui) {
        auto rows = ui.moderation.listForDepartment(s, askFilter(s, ui));
        printRows(ui, rows, true);
        for (const SubmissionRow& r : rows)
            if (r.mark) ui.out << "  [" << r.submissionId << "] feedback: " << r.mark->feedback << "\n";
    });
    add(g, "Approve one mark", Function::MODERATE_MARKS, [](Session& s, UiContext& ui) {
        RowFilter f; f.status = SubmissionStatus::MARKED;
        auto rows = ui.moderation.listForDepartment(s, f);
        printRows(ui, rows, true);
        auto r = pickRow(ui, rows, "Submission id to approve");
        if (!r || !ui.in.confirm("Approve this mark")) return;
        ui.moderation.approve(s, r->submissionId);
        ui.out << "  Approved (status Moderated). It is not visible to the student until released.\n";
    });
    add(g, "Approve all marked submissions of an assignment", Function::MODERATE_MARKS, [](Session& s, UiContext& ui) {
        auto a = pickAssignment(s, ui);
        if (!a) return;
        const int n = ui.moderation.countAwaitingModeration(s, a->id);
        if (n == 0) { ui.out << "  Nothing is waiting for moderation.\n"; return; }
        if (!ui.in.confirm("Approve " + std::to_string(n) + " mark(s)")) return;
        ui.out << "  " << ui.moderation.approveAll(s, a->id) << " mark(s) approved.\n";
    });
    add(g, "Adjust a mark", Function::MODERATE_MARKS, [](Session& s, UiContext& ui) {
        RowFilter f; f.status = SubmissionStatus::MARKED;
        auto rows = ui.moderation.listForDepartment(s, f);
        printRows(ui, rows, true);
        auto r = pickRow(ui, rows, "Submission id to adjust");
        if (!r) return;
        const double fm = ui.in.readMark("New final mark (0-" + std::to_string(r->maxMark) + "): ", r->maxMark);
        const std::string reason = ui.in.readText("Reason (at least 10 characters): ", 10, 500);
        if (!ui.in.confirm("Replace the final mark")) return;
        ui.moderation.adjust(s, r->submissionId, fm, reason);
        ui.out << "  Adjusted and approved. The change is in the audit log.\n";
    });
    add(g, "Return a mark to the lecturer", Function::MODERATE_MARKS, [](Session& s, UiContext& ui) {
        RowFilter f; f.status = SubmissionStatus::MARKED;
        auto rows = ui.moderation.listForDepartment(s, f);
        printRows(ui, rows, true);
        auto r = pickRow(ui, rows, "Submission id to return");
        if (!r) return;
        const std::string c = ui.in.readText("Comment for the lecturer: ", 1, 500);
        ui.moderation.returnForRemarking(s, r->submissionId, c);
        ui.out << "  Returned for remarking.\n";
    });
    add(g, "Release one moderated mark", Function::RELEASE_MARKS, [](Session& s, UiContext& ui) {
        RowFilter f; f.status = SubmissionStatus::MODERATED;
        auto rows = ui.moderation.listForDepartment(s, f);
        printRows(ui, rows, true);
        auto r = pickRow(ui, rows, "Submission id to release");
        if (!r || !ui.in.confirm("Release this mark to the student")) return;
        ui.moderation.release(s, r->submissionId);
        ui.out << "  Released.\n";
    });
    add(g, "Release all moderated marks of an assignment", Function::RELEASE_MARKS, [](Session& s, UiContext& ui) {
        auto a = pickAssignment(s, ui);
        if (!a) return;
        const int n = ui.moderation.countReleasable(s, a->id);
        if (n == 0) { ui.out << "  No moderated marks are waiting for release.\n"; return; }
        if (!ui.in.confirm("Release " + std::to_string(n) + " mark(s) to students")) return;
        ui.out << "  " << ui.moderation.releaseAll(s, a->id) << " mark(s) released.\n";
    });
    add(g, "Correct a released mark", Function::CORRECT_MARK, [](Session& s, UiContext& ui) {
        RowFilter f; f.status = SubmissionStatus::RELEASED;
        auto rows = ui.moderation.listForDepartment(s, f);
        printRows(ui, rows, true);
        auto r = pickRow(ui, rows, "Submission id to correct");
        if (!r) return;
        const double fm = ui.in.readMark("Corrected final mark (0-" + std::to_string(r->maxMark) + "): ", r->maxMark);
        const std::string reason = ui.in.readText("Reason (at least 10 characters): ", 10, 500);
        if (!ui.in.confirm("Correct the released mark")) return;
        ui.moderation.correctReleased(s, r->submissionId, fm, reason);
        ui.out << "  Corrected. The status stays Released and the change is in the audit log.\n";
    });
    return g;
}

CommandGroup auditGroup() {
    CommandGroup g{"Audit log", {}};
    add(g, "View the audit log of my department", Function::VIEW_AUDIT_LOG, [](Session& s, UiContext& ui) {
        AuditFilter f;
        ui.out << "  Filter: 0 none, 1 one assignment, 2 one date range\n";
        const int mode = ui.in.readInt("Filter: ", 0, 2);
        if (mode == 1) { auto a = pickAssignment(s, ui); if (!a) return; f.assignmentId = a->id; }
        if (mode == 2) { f.from = ui.in.readDateTime("From"); f.to = ui.in.readDateTime("To"); }
        auto rows = ui.audit.query(s, f);
        if (rows.empty()) { ui.out << "  No audit records.\n"; return; }
        for (const AuditRow& r : rows)
            ui.out << "  " << toLocalDisplay(r.record.changedAt) << "  " << r.changedByName << "  "
                   << toString(r.record.actionType) << "  " << r.courseCode << "/" << r.assignmentTitle << "  "
                   << r.studentName << "  " << toString(r.record.fieldName) << ": "
                   << (r.record.oldValue ? *r.record.oldValue : std::string("(none)")) << " -> "
                   << r.record.newValue << "  reason: " << r.record.reason << "\n";
    });
    return g;
}

CommandGroup studentGroup() {
    CommandGroup g{"My courses and work", {}};
    add(g, "View my courses and assignments", Function::VIEW_COURSES, [](Session& s, UiContext& ui) {
        auto list = ui.submissions.listAssignmentsForStudent(s);
        if (list.empty()) { ui.out << "  You are not enrolled in any course with assignments.\n"; return; }
        ui.out << "\n  " << fit("ID", 5) << fit("Course", 10) << fit("Assignment", 24) << fit("Deadline", 17)
               << fit("Your deadline", 17) << "Status\n";
        for (const auto& v : list) {
            const bool ext = v.effectiveDeadline != v.assignment.deadline;
            ui.out << "  " << fit(std::to_string(v.assignment.id), 5) << fit(v.course.code, 10)
                   << fit(v.assignment.title, 24) << fit(toLocalDisplay(v.assignment.deadline), 17)
                   << fit(ext ? toLocalDisplay(v.effectiveDeadline) : "-", 17) << v.statusText << "\n";
        }
    });
    add(g, "Submit or resubmit work", Function::SUBMIT_WORK, [](Session& s, UiContext& ui) {
        auto list = ui.submissions.listAssignmentsForStudent(s);
        if (list.empty()) { ui.out << "  You have no assignments.\n"; return; }
        for (const auto& v : list)
            ui.out << "  " << v.assignment.id << "  " << v.course.code << "  " << v.assignment.title << "  (due "
                   << toLocalDisplay(v.effectiveDeadline) << ", " << v.statusText << ")\n";
        auto id = ui.in.readId("Assignment id");
        if (!id) return;
        const std::string path = ui.in.readText("Path of the file (PDF, DOCX, TXT, ZIP, PNG or JPG; up to 10 MB): ", 1, 500);
        SubmissionPreview p = ui.submissions.preview(s, *id, path);
        bool lateConfirmed = false;
        if (p.late) {
            ui.out << "\n  WARNING: this work is LATE. Your deadline was " << toLocalDisplay(p.effectiveDeadline)
                   << ".\n  Days late: " << p.daysLate << "\n";
            if (p.zeroMark)
                ui.out << "  After 3 days late the final mark is ZERO. Your work is still accepted and\n"
                          "  your lecturer will mark it and give feedback; only the Head of Department can change the zero.\n";
            else
                ui.out << "  Penalty: " << formatMark(p.penaltyMarks) << " mark(s) out of " << p.maxMark
                       << " (10% of the maximum mark per day).\n";
            ui.out << "  You will not be able to replace late work.\n";
            if (!ui.in.confirm("Submit anyway")) { ui.out << "  Nothing was saved.\n"; return; }
            lateConfirmed = true;
        } else {
            ui.out << "\n  " << (p.isResubmission ? "This will replace your earlier submission (version "
                                                    + std::to_string(p.nextVersionNo) + "); older versions are kept.\n"
                                                  : "This is your first submission for this assignment.\n");
            if (!ui.in.confirm("Submit this file")) { ui.out << "  Nothing was saved.\n"; return; }
        }
        Receipt r = ui.submissions.submit(s, *id, path, lateConfirmed);
        ui.out << "\n  RECEIPT\n  Time:    " << toLocalDisplay(r.time) << "\n  File:    " << r.fileName
               << "\n  Size:    " << r.sizeBytes << " bytes\n  Version: " << r.versionNo
               << "\n  Status:  " << studentStatusText(r.status) << "\n";
    });
    add(g, "View my submissions and released marks", Function::VIEW_RELEASED_MARKS, [](Session& s, UiContext& ui) {
        auto list = ui.submissions.listForStudent(s);
        if (list.empty()) { ui.out << "  You have not submitted any work yet.\n"; return; }
        for (const auto& v : list) {
            ui.out << "\n  " << v.courseCode << " - " << v.assignmentTitle << "\n    Handed in: "
                   << toLocalDisplay(v.uploadedAt) << " (version " << v.versionNo << ")\n    Status: " << v.statusText << "\n";
            if (v.released)
                ui.out << "    Mark: " << formatMark(v.finalMark) << " out of " << v.maxMark << "  (raw "
                       << formatMark(v.rawMark) << ", days late " << v.daysLate << ", penalty " << formatMark(v.penalty)
                       << ")\n    Feedback: " << v.feedback << "\n";
        }
    });
    return g;
}

} // namespace

std::vector<CommandGroup> buildCatalogue() {
    std::vector<CommandGroup> all;
    all.push_back(studentGroup());
    all.push_back(assignmentGroup());
    all.push_back(markingGroup());
    all.push_back(moderationGroup());
    all.push_back(auditGroup());
    all.push_back(accountsAdminGroup());
    all.push_back(structureGroup());
    all.push_back(backupGroup());
    all.push_back(accountGroup());
    return all;
}

} // namespace lms
