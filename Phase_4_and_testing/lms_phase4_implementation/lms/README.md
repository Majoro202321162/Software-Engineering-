# Moderated Learning Management System (Phase 4: Implementation)

C++17 console application for a secondary school: lecturers set and mark assignments, a Head of Department (HoD)
moderates and releases marks, students see a mark only after release. SQLite storage, Argon2id password hashing (libsodium),
layered architecture exactly as in the Phase 3 design. Works fully offline.

Student: Lehlohonolo Majoro (202321162) - CS4433 Software Engineering, National University of Lesotho.

## Requirements
* A C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+), CMake 3.16+
* SQLite 3.27+ (needs `VACUUM INTO` for backups, upsert) and libsodium
  * Ubuntu 22.04: `sudo apt install build-essential cmake libsqlite3-dev libsodium-dev`
  * Windows: `vcpkg install sqlite3 libsodium` then pass `-DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake`

## Build, test, run
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure     # 153 unit/integration checks + end-to-end console scenario
./build/lms                                    # or: ./build/lms --db /path/lms.db --store /path/submissions
```
**First start:** the program creates the database from `schema/schema.sql` (embedded in the program) and one
administrator, `admin`, and prints its temporary password once. Every temporary password must be changed at the next login.

Typical order of work: Administrator creates departments, courses, accounts, assigns lecturers and enrols students ->
Lecturer creates assignments -> Student submits -> Lecturer marks -> HoD approves/adjusts/returns, then releases -> Student reads the mark.

## Project layout
| Folder | Content (Phase 3 reference) |
|---|---|
| `schema/schema.sql` | The SQLite schema: 12 tables, CHECK constraints, indexes, triggers (Section 5.5) |
| `src/domain` | `User` hierarchy, `UserFactory`, enums, record types (Fig. 3.1, 3.2) |
| `src/common` | Exception hierarchy (Fig. 3.7), UTC time helpers |
| `src/infra` | `IClock`, `IPasswordHasher` (Argon2id), `IFileStore` (Fig. 3.6) |
| `src/data` | `Database`, `Statement`, RAII `Transaction`, repository interfaces and SQLite implementations (Fig. 3.5, 3.6) |
| `src/business` | Services, `IPenaltyStrategy`, `SubmissionLifecycle`, `PermissionPolicy` (Fig. 3.3, 3.4) |
| `src/presentation` | `ConsoleApp`, `Menu`/`ICommand`/`MenuBuilder`, `InputReader`, command catalogue (Fig. 3.3) |
| `src/main.cpp` | Composition root: the only place that names SQLite/libsodium classes (DIP) |
| `tests/` | `test_core.cpp` (dependency-free unit + integration tests), `e2e_console.py` (system scenario) |
| `tools/gen_schema.py` | Regenerates `src/data/SchemaSql.cpp` from `schema/schema.sql` after a schema change |
| `docs/CODING_STANDARD.md` | Coding standard |

## Design decisions (DD-01 to DD-11) in the code
DD-01 layered architecture; DD-02 `TenPercentPerDayPenalty` behind `IPenaltyStrategy`; DD-03 repositories only;
DD-04 abstract `User` + `UserFactory`; DD-05 `ICommand` objects, menus built from `permittedFunctions()`;
DD-06 `SubmissionLifecycle` transition table (T3-T12); DD-07 mark + status + audit in one RAII transaction
(`IUnitOfWork`); DD-08 UTC ISO-8601 text; DD-09 separate `marks` table; DD-10 single `users` table with `role`;
DD-11 audit append-only by interface (no update/delete) **and** by database triggers.

## Where each important rule is enforced (Table 5.17)
* Role permissions: menu only offers permitted functions, and every service calls `PermissionPolicy::require()`; scope
  checks (own course, own department) in `AccessScope`; refusals go to `event_log`.
* Student sees marks only when `RELEASED` (BR-02): `SubmissionService::listForStudent()` copies mark fields only for that status.
* HoD cannot moderate own mark, only own department (BR-10); only `MODERATED` can be released (BR-11).
* Penalty (BR-03 to BR-07): `AssignmentService::effectiveDeadline`, `TenPercentPerDayPenalty`, applied by `MarkingService`.
* Mark/audit atomicity (NFR-09): tested by fault injection in `testFaultInjection`.

## Deliberate differences from the Phase 3 document (record these through change control)
1. `IUnitOfWork`/`ITransaction` interfaces wrap the RAII `Transaction` so services never include SQLite classes.
2. `SubmissionService::submit()` takes an extra `lateConfirmed` flag, so late work is never saved without the student's confirmation even if the clock passes the deadline between preview and submit.
3. `IPasswordHasher::generateTemporaryPassword()` added (temporary passwords, FR-04/FR-07); `AuthService::validateNewPassword()` enforces 8+ characters with a letter and a digit.
4. `Function` enumeration gained `CHANGE_PASSWORD` (all roles) and `EXPORT_MARKS` (FR-33).
5. `IFileStore` gained `remove()` (undo a copied file if the database write fails), `readTextPreview()` and `copyStoreTo()`; `store()` takes assignment and student ids because the submission row may not exist yet (the path is "built from the assignment, student and version numbers", Table 2.4).
6. Menu actions are `ActionCommand` objects (one per action, created in `Commands.cpp`) instead of one named subclass each; behaviour and the `ICommand` interface are as designed.
7. `marks.penalty` for a zero (4+ days late) stores the whole raw mark removed; `penalty()` returns the maximum mark in that case and `givesZero()` marks the rule explicitly.
8. A lecturer can only be assigned to a course of the lecturer's own department (so that BR-10 department moderation stays consistent).
9. "Edit user" changes name/department only; the role of an account never changes (BR-01).
10. Added `AssignmentService::enrolledStudents()` and `UserService::createInitialAdministrator()` (first start).

## Security notes (NFR-05 to NFR-07)
Passwords: Argon2id with a unique random salt per hash (libsodium interactive limits, tuned in one place, `LibsodiumHasher.cpp`);
a dummy hash is checked for unknown users so both failures look and time alike. Passwords are never logged or stored.
SQL: only prepared statements with bound parameters. Files: the destination path is built by the system; the typed path is only read.
CSV export neutralises cells starting with `= + - @`.

## Backup and restore (FR-34, NFR-15)
Administrator menu -> *Back up data* writes `<folder>/lms_backup_<time>/lms.db` (consistent copy via `VACUUM INTO`) and `submissions/`.
Restore: stop the program, copy `lms.db` and the `submissions` folder back to the program folder (or point `--db`/`--store` at them).

## Known limitations
* Password entry is hidden on a real terminal; when input is redirected from a file it is read as plain text.
* Students must type the file path (console application); there is no file browser.
* Course/department editing offers rename/close only; nothing is ever deleted (BR-13).
* Not included here (later phases): GoogleTest/Catch2 port of the tests, coverage report, deployment diagram, v1.1 change request CR1.
