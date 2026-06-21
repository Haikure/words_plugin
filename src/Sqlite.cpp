#include "Sqlite.h"

#include <QtGlobal>
#include <cstdio>

namespace word {

// ============================ Stmt ============================

Stmt::Stmt(sqlite3* db, const QString& sql) {
    if (!db) return;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_prepare_v2(db, utf8.constData(), utf8.size(), &m_stmt, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "[word_plugin] prepare failed: %s\n", sqlite3_errmsg(db));
        m_stmt = nullptr;
    }
}

Stmt::~Stmt() {
    if (m_stmt) sqlite3_finalize(m_stmt);
}

Stmt::Stmt(Stmt&& other) noexcept : m_stmt(other.m_stmt) {
    other.m_stmt = nullptr;
}

Stmt& Stmt::operator=(Stmt&& other) noexcept {
    if (this != &other) {
        if (m_stmt) sqlite3_finalize(m_stmt);
        m_stmt = other.m_stmt;
        other.m_stmt = nullptr;
    }
    return *this;
}

void Stmt::bind(int index, int value) {
    if (m_stmt) sqlite3_bind_int(m_stmt, index, value);
}

void Stmt::bind(int index, qint64 value) {
    if (m_stmt) sqlite3_bind_int64(m_stmt, index, value);
}

void Stmt::bind(int index, const QString& value) {
    if (!m_stmt) return;
    const QByteArray utf8 = value.toUtf8();
    // SQLITE_TRANSIENT: 让 sqlite 自行拷贝，避免临时缓冲失效
    sqlite3_bind_text(m_stmt, index, utf8.constData(), utf8.size(), SQLITE_TRANSIENT);
}

bool Stmt::step() {
    if (!m_stmt) return false;
    return sqlite3_step(m_stmt) == SQLITE_ROW;
}

int Stmt::columnInt(int col) const {
    return m_stmt ? sqlite3_column_int(m_stmt, col) : 0;
}

qint64 Stmt::columnInt64(int col) const {
    return m_stmt ? sqlite3_column_int64(m_stmt, col) : 0;
}

QString Stmt::columnText(int col) const {
    if (!m_stmt) return QString();
    const unsigned char* text = sqlite3_column_text(m_stmt, col);
    if (!text) return QString();
    return QString::fromUtf8(reinterpret_cast<const char*>(text));
}

void Stmt::reset() {
    if (m_stmt) {
        sqlite3_reset(m_stmt);
        sqlite3_clear_bindings(m_stmt);
    }
}

// ============================ Sqlite ============================

Sqlite::~Sqlite() {
    close();
}

bool Sqlite::open(const QString& path, bool readOnly) {
    close();
    const int flags = readOnly ? SQLITE_OPEN_READONLY
                               : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    const QByteArray utf8 = path.toUtf8();
    if (sqlite3_open_v2(utf8.constData(), &m_db, flags, nullptr) != SQLITE_OK) {
        std::fprintf(stderr, "[word_plugin] open failed: %s\n",
                     m_db ? sqlite3_errmsg(m_db) : "unknown");
        close();
        return false;
    }
    // 嵌入式单进程场景下的轻量化设置
    if (!readOnly) {
        exec(QStringLiteral("PRAGMA journal_mode=WAL"));
        exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    }
    return true;
}

void Sqlite::close() {
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool Sqlite::exec(const QString& sql) {
    if (!m_db) return false;
    char* errMsg = nullptr;
    const QByteArray utf8 = sql.toUtf8();
    if (sqlite3_exec(m_db, utf8.constData(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::fprintf(stderr, "[word_plugin] exec failed: %s\n", errMsg ? errMsg : "?");
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

QString Sqlite::lastError() const {
    return m_db ? QString::fromUtf8(sqlite3_errmsg(m_db)) : QStringLiteral("db not open");
}

} // namespace word
