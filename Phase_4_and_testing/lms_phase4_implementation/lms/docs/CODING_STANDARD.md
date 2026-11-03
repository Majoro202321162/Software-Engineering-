# Coding standard (Phase 4)

* Language: C++17, no compiler extensions, warnings on (`-Wall -Wextra -Wpedantic`, MSVC `/W4`); zero warnings accepted.
* Naming: `PascalCase` classes and enums; `camelCase` functions and variables; trailing underscore for private data
  members (`db_`); `kConstant` for constants; interfaces start with `I` (`IClock`).
* Layers (Phase 3, Section 2.2): a layer calls only the layer below. `presentation` -> `business` -> `data`/`infra`.
  SQL text exists only in `src/data`. `sqlite3.h` is included only in `Database.cpp`; `sodium.h` only in `LibsodiumHasher.cpp`.
* Every service method starts with `PermissionPolicy::require()` (and a scope check where a record belongs to a course or department).
* Memory: no raw `new`/`delete`; `std::unique_ptr` for ownership, references for non-owning dependencies.
* Errors: throw `LmsException` subclasses; never `catch (...)` except to clean up and rethrow; destructors never throw.
* SQL: parameters are always bound (`Statement::bind`); user text is never concatenated into SQL.
* Time: only `IClock::now()`; all times are UTC; local time only in `toLocalDisplay()` / `parseLocal()`.
* Functions: aim for cyclomatic complexity <= 10 (NFR-12); comments explain *why* and cite the rule (BR-xx, FR-xx).
* Formatting: 4 spaces, no tabs, lines <= 110 characters, `#pragma once`.
* Commits: short imperative subject; feature branches merged by pull request; tags `v1.0`, `v1.1`.
