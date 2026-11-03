-- =====================================================================
-- Moderated LMS - SQLite schema, version 1 (Phase 3, Section 5.5)
-- Single source of the database structure. Changes go through change
-- control (risk R16). Times are UTC ISO-8601 text (e.g. 2026-11-02T14:30:00Z).
-- The application executes PRAGMA foreign_keys = ON on every connection.
-- =====================================================================

CREATE TABLE departments (
    department_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name          TEXT NOT NULL UNIQUE CHECK (length(name) BETWEEN 2 AND 100)
);

CREATE TABLE users (
    user_id              INTEGER PRIMARY KEY AUTOINCREMENT,
    username             TEXT NOT NULL UNIQUE COLLATE NOCASE
                              CHECK (length(username) BETWEEN 3 AND 30),
    full_name            TEXT NOT NULL CHECK (length(full_name) BETWEEN 2 AND 100),
    role                 TEXT NOT NULL CHECK (role IN ('ADMINISTRATOR','LECTURER',
                                                       'HEAD_OF_DEPARTMENT','STUDENT')),
    department_id        INTEGER REFERENCES departments(department_id),
    student_number       TEXT UNIQUE,
    password_hash        TEXT NOT NULL,
    must_change_password INTEGER NOT NULL DEFAULT 1 CHECK (must_change_password IN (0,1)),
    is_active            INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0,1)),
    failed_attempts      INTEGER NOT NULL DEFAULT 0 CHECK (failed_attempts >= 0),
    locked_until         TEXT,
    last_login_at        TEXT,
    CHECK ((role IN ('LECTURER','HEAD_OF_DEPARTMENT')
              AND department_id IS NOT NULL AND student_number IS NULL)
        OR (role = 'STUDENT' AND student_number IS NOT NULL AND department_id IS NULL)
        OR (role = 'ADMINISTRATOR' AND department_id IS NULL AND student_number IS NULL))
);

-- at most one active HoD per department (FR-08)
CREATE UNIQUE INDEX ux_one_active_hod ON users(department_id)
    WHERE role = 'HEAD_OF_DEPARTMENT' AND is_active = 1;

CREATE TABLE courses (
    course_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    code          TEXT NOT NULL UNIQUE CHECK (length(code) BETWEEN 2 AND 20),
    title         TEXT NOT NULL CHECK (length(title) BETWEEN 2 AND 100),
    department_id INTEGER NOT NULL REFERENCES departments(department_id),
    is_active     INTEGER NOT NULL DEFAULT 1 CHECK (is_active IN (0,1))
);

CREATE TABLE course_lecturers (
    course_id   INTEGER NOT NULL REFERENCES courses(course_id),
    lecturer_id INTEGER NOT NULL REFERENCES users(user_id),
    PRIMARY KEY (course_id, lecturer_id)
);

CREATE TABLE enrolments (
    course_id     INTEGER NOT NULL REFERENCES courses(course_id),
    student_id    INTEGER NOT NULL REFERENCES users(user_id),
    enrolled_at   TEXT NOT NULL,
    unenrolled_at TEXT,
    PRIMARY KEY (course_id, student_id)
);

CREATE TABLE assignments (
    assignment_id INTEGER PRIMARY KEY AUTOINCREMENT,
    course_id     INTEGER NOT NULL REFERENCES courses(course_id),
    title         TEXT NOT NULL CHECK (length(title) BETWEEN 2 AND 100),
    description   TEXT NOT NULL DEFAULT '' CHECK (length(description) <= 2000),
    deadline      TEXT NOT NULL,
    max_mark      INTEGER NOT NULL CHECK (max_mark BETWEEN 1 AND 100),
    is_closed     INTEGER NOT NULL DEFAULT 0 CHECK (is_closed IN (0,1)),
    created_by    INTEGER NOT NULL REFERENCES users(user_id),
    created_at    TEXT NOT NULL
);

CREATE TABLE extensions (
    extension_id  INTEGER PRIMARY KEY AUTOINCREMENT,
    assignment_id INTEGER NOT NULL REFERENCES assignments(assignment_id),
    student_id    INTEGER NOT NULL REFERENCES users(user_id),
    new_deadline  TEXT NOT NULL,
    reason        TEXT NOT NULL CHECK (length(reason) >= 10),
    granted_by    INTEGER NOT NULL REFERENCES users(user_id),
    granted_at    TEXT NOT NULL
);

CREATE TABLE submissions (
    submission_id INTEGER PRIMARY KEY AUTOINCREMENT,
    assignment_id INTEGER NOT NULL REFERENCES assignments(assignment_id),
    student_id    INTEGER NOT NULL REFERENCES users(user_id),
    status        TEXT NOT NULL CHECK (status IN ('SUBMITTED','LATE','MARKED',
                                                  'RETURNED','MODERATED','RELEASED')),
    UNIQUE (assignment_id, student_id)
);

CREATE TABLE submission_versions (
    version_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    submission_id   INTEGER NOT NULL REFERENCES submissions(submission_id),
    version_no      INTEGER NOT NULL CHECK (version_no >= 1),
    file_name       TEXT NOT NULL,
    stored_path     TEXT NOT NULL,
    file_size_bytes INTEGER NOT NULL CHECK (file_size_bytes BETWEEN 1 AND 10485760),
    uploaded_at     TEXT NOT NULL,
    UNIQUE (submission_id, version_no)
);

CREATE TABLE marks (
    submission_id INTEGER PRIMARY KEY REFERENCES submissions(submission_id),
    raw_mark      REAL NOT NULL CHECK (raw_mark >= 0
                                       AND raw_mark * 2 = CAST(raw_mark * 2 AS INTEGER)),
    days_late     INTEGER NOT NULL DEFAULT 0 CHECK (days_late >= 0),
    penalty       REAL NOT NULL DEFAULT 0 CHECK (penalty >= 0),
    final_mark    REAL NOT NULL CHECK (final_mark >= 0),
    feedback      TEXT NOT NULL CHECK (length(feedback) BETWEEN 10 AND 2000),
    marked_by     INTEGER NOT NULL REFERENCES users(user_id),
    marked_at     TEXT NOT NULL,
    hod_comment   TEXT,
    moderated_by  INTEGER REFERENCES users(user_id),
    moderated_at  TEXT,
    released_by   INTEGER REFERENCES users(user_id),
    released_at   TEXT
);

CREATE TABLE audit_records (
    audit_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    submission_id INTEGER NOT NULL REFERENCES submissions(submission_id),
    changed_by    INTEGER NOT NULL REFERENCES users(user_id),
    changed_at    TEXT NOT NULL,
    action_type   TEXT NOT NULL CHECK (action_type IN ('INITIAL_MARKING','REMARK',
                                       'MODERATION_ADJUSTMENT','POST_RELEASE_CORRECTION')),
    field_name    TEXT NOT NULL CHECK (field_name IN ('RAW_MARK','FEEDBACK','FINAL_MARK')),
    old_value     TEXT,
    new_value     TEXT NOT NULL,
    reason        TEXT NOT NULL CHECK (length(reason) >= 10)
);

CREATE TABLE event_log (
    event_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     INTEGER REFERENCES users(user_id),
    event_type  TEXT NOT NULL CHECK (event_type IN ('ACCESS_DENIED','LOGIN_OK',
                                    'LOGIN_FAILED','LOGIN_LOCKED','FILE_ERROR','DB_ERROR')),
    detail      TEXT NOT NULL CHECK (length(detail) <= 500),
    occurred_at TEXT NOT NULL
);

-- indexes (Section 5.7)
CREATE INDEX ix_courses_department  ON courses(department_id);
CREATE INDEX ix_clecturers_lecturer ON course_lecturers(lecturer_id);
CREATE INDEX ix_enrol_student       ON enrolments(student_id);
CREATE INDEX ix_assignments_course  ON assignments(course_id);
CREATE INDEX ix_extensions_lookup   ON extensions(assignment_id, student_id);
CREATE INDEX ix_sub_assign_status   ON submissions(assignment_id, status);
CREATE INDEX ix_sub_student         ON submissions(student_id);
CREATE INDEX ix_marks_released      ON marks(released_at);
CREATE INDEX ix_audit_submission    ON audit_records(submission_id, changed_at);

-- audit records are append-only (FR-29)
CREATE TRIGGER trg_audit_no_update BEFORE UPDATE ON audit_records
BEGIN SELECT RAISE(ABORT, 'Audit records cannot be changed'); END;

CREATE TRIGGER trg_audit_no_delete BEFORE DELETE ON audit_records
BEGIN SELECT RAISE(ABORT, 'Audit records cannot be deleted'); END;

-- a mark may not exceed the maximum mark of its assignment
CREATE TRIGGER trg_marks_max_insert BEFORE INSERT ON marks
WHEN NEW.raw_mark > (SELECT a.max_mark FROM submissions s
                       JOIN assignments a ON a.assignment_id = s.assignment_id
                      WHERE s.submission_id = NEW.submission_id)
  OR NEW.final_mark > (SELECT a.max_mark FROM submissions s
                         JOIN assignments a ON a.assignment_id = s.assignment_id
                        WHERE s.submission_id = NEW.submission_id)
BEGIN SELECT RAISE(ABORT, 'Mark is above the maximum mark'); END;

CREATE TRIGGER trg_marks_max_update BEFORE UPDATE OF raw_mark, final_mark ON marks
WHEN NEW.raw_mark > (SELECT a.max_mark FROM submissions s
                       JOIN assignments a ON a.assignment_id = s.assignment_id
                      WHERE s.submission_id = NEW.submission_id)
  OR NEW.final_mark > (SELECT a.max_mark FROM submissions s
                         JOIN assignments a ON a.assignment_id = s.assignment_id
                        WHERE s.submission_id = NEW.submission_id)
BEGIN SELECT RAISE(ABORT, 'Mark is above the maximum mark'); END;

-- history is never deleted (BR-13)
CREATE TRIGGER trg_users_no_delete BEFORE DELETE ON users
BEGIN SELECT RAISE(ABORT, 'Deactivate instead of deleting'); END;

CREATE TRIGGER trg_courses_no_delete BEFORE DELETE ON courses
BEGIN SELECT RAISE(ABORT, 'Deactivate instead of deleting'); END;

CREATE TRIGGER trg_assignments_no_delete BEFORE DELETE ON assignments
BEGIN SELECT RAISE(ABORT, 'Close instead of deleting'); END;

CREATE TRIGGER trg_submissions_no_delete BEFORE DELETE ON submissions
BEGIN SELECT RAISE(ABORT, 'Submissions are never deleted'); END;

CREATE TRIGGER trg_marks_no_delete BEFORE DELETE ON marks
BEGIN SELECT RAISE(ABORT, 'Marks are never deleted'); END;
