#include "data/Database.h"
#include "common/Exceptions.h"

#include <sqlite3.h>

namespace lms {

// ---- Statement -----------------------------------------------------------
Statement::Statement(Database& db, const std::string& sql) : db_(&db), stmt_(nullptr) {
    if (sqlite3_prepare_v2(db.handle(), sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK)
        db.throwError("prepare");
}

Statement::Statement(Statement&& o) noexcept : db_(o.db_), stmt_(o.stmt_) { o.stmt_ = nullptr; }

Statement::~Statement() { if (stmt_) sqlite3_finalize(stmt_); }

void Statement::bind(int i, int v) {
    if (sqlite3_bind_int(stmt_, i, v) != SQLITE_OK) db_->throwError("bind");
}
void Statement::bind(int i, long long v) {
    if (sqlite3_bind_int64(stmt_, i, v) != SQLITE_OK) db_->throwError("bind");
}
void Statement::bind(int i, double v) {
    if (sqlite3_bind_double(stmt_, i, v) != SQLITE_OK) db_->throwError("bind");
}
void Statement::bind(int i, bool v) { bind(i, v ? 1 : 0); }
void Statement::bind(int i, const std::string& v) {
    if (sqlite3_bind_text(stmt_, i, v.c_str(), static_cast<int>(v.size()), SQLITE_TRANSIENT) != SQLITE_OK)
        db_->throwError("bind");
}
void Statement::bind(int i, const char* v) { bind(i, std::string(v)); }
void Statement::bindNull(int i) {
    if (sqlite3_bind_null(stmt_, i) != SQLITE_OK) db_->throwError("bind");
}

bool Statement::step() {
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    db_->throwError("step");
}

void Statement::reset() { sqlite3_reset(stmt_); sqlite3_clear_bindings(stmt_); }

bool Statement::isNull(int c) const { return sqlite3_column_type(stmt_, c) == SQLITE_NULL; }
int Statement::columnInt(int c) const { return sqlite3_column_int(stmt_, c); }
long long Statement::columnInt64(int c) const { return sqlite3_column_int64(stmt_, c); }
double Statement::columnDouble(int c) const { return sqlite3_column_double(stmt_, c); }

std::string Statement::columnText(int c) const {
    const unsigned char* t = sqlite3_column_text(stmt_, c);
    return t ? reinterpret_cast<const char*>(t) : std::string();
}
std::optional<std::string> Statement::columnOptText(int c) const {
    if (isNull(c)) return std::nullopt;
    return columnText(c);
}
std::optional<int> Statement::columnOptInt(int c) const {
    if (isNull(c)) return std::nullopt;
    return columnInt(c);
}

// ---- Database ------------------------------------------------------------
Database::~Database() { close(); }

void Database::throwError(const std::string& context) const {
    const std::string msg = handle_ ? sqlite3_errmsg(handle_) : "database not open";
    throw DataAccessException(context + ": " + msg);
}

void Database::open(const std::string& path) {
    close();
    if (sqlite3_open(path.c_str(), &handle_) != SQLITE_OK) {
        const std::string msg = handle_ ? sqlite3_errmsg(handle_) : "cannot open";
        close();
        throw DataAccessException("open: " + msg);
    }
    sqlite3_busy_timeout(handle_, 3000);
    exec("PRAGMA foreign_keys = ON");                 // on every connection (NFR-09)
    Statement check = prepare("SELECT count(*) FROM sqlite_master WHERE type='table' AND name='users'");
    check.step();
    if (check.columnInt(0) == 0) {
        Transaction tx = begin();
        exec(schemaSql());
        exec("PRAGMA user_version = 1");
        tx.commit();
    }
}

void Database::close() {
    if (handle_) { sqlite3_close(handle_); handle_ = nullptr; }
    txDepth_ = 0;
    txFailed_ = false;
}

void Database::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(handle_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        const std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw DataAccessException("exec: " + msg);
    }
}

long long Database::lastInsertId() const { return sqlite3_last_insert_rowid(handle_); }

Transaction Database::begin() { return Transaction(*this); }

void Database::backupTo(const std::string& targetFile) {
    if (txDepth_ != 0) throw DataAccessException("backup: a transaction is open");
    Statement st = prepare("VACUUM INTO ?");
    st.bind(1, targetFile);
    st.step();
}

void Database::beginTx() {
    if (txDepth_ == 0) { exec("BEGIN IMMEDIATE"); txFailed_ = false; }
    ++txDepth_;
}

void Database::commitTx() {
    --txDepth_;
    if (txDepth_ > 0) return;
    if (txFailed_) {
        txFailed_ = false;
        exec("ROLLBACK");
        throw DataAccessException("commit: an inner operation was rolled back");
    }
    exec("COMMIT");
}

void Database::rollbackTx() noexcept {
    if (txDepth_ == 0) return;
    txFailed_ = true;
    --txDepth_;
    if (txDepth_ == 0) {
        try { exec("ROLLBACK"); } catch (...) {}
        txFailed_ = false;
    }
}

// ---- Transaction ---------------------------------------------------------
Transaction::Transaction(Database& db) : db_(&db) { db_->beginTx(); }

Transaction::Transaction(Transaction&& o) noexcept
    : db_(o.db_), committed_(o.committed_), active_(o.active_) { o.active_ = false; }

void Transaction::commit() {
    if (!active_ || committed_) return;
    committed_ = true;
    db_->commitTx();
}

Transaction::~Transaction() {
    if (active_ && !committed_) db_->rollbackTx();
}

} // namespace lms
