#pragma once

// ---------------------------------------------------------------------------
// Sqlite —— sqlite3 C API 的极薄 RAII 封装
//
// 设计目标：在 rk3326 这种低算力设备上保持轻量、零额外依赖（不引入 QtSql）。
// 只提供本插件需要的能力：打开/关闭、执行无返回语句、预编译语句（Stmt）取值。
// 所有取值接口对列越界/类型为 NULL 的情况都返回安全默认值。
// ---------------------------------------------------------------------------

#include <sqlite3.h>

#include <QString>
#include <QVariant>

namespace word {

// 预编译语句的 RAII 包装：构造即 prepare，析构即 finalize。
class Stmt {
public:
    Stmt() = default;
    Stmt(sqlite3* db, const QString& sql);
    ~Stmt();

    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;
    Stmt(Stmt&& other) noexcept;
    Stmt& operator=(Stmt&& other) noexcept;

    bool isValid() const { return m_stmt != nullptr; }

    // 参数绑定（占位符从 1 开始）
    void bind(int index, int value);
    void bind(int index, qint64 value);
    void bind(int index, const QString& value);

    // 单步执行；有数据行返回 true，结束返回 false
    bool step();

    // 取列值（列从 0 开始）
    int      columnInt(int col) const;
    qint64   columnInt64(int col) const;
    QString  columnText(int col) const;

    // 重置以便复用（重新 bind 后再 step）
    void reset();

private:
    sqlite3_stmt* m_stmt = nullptr;
};

// 数据库连接的 RAII 包装。
class Sqlite {
public:
    Sqlite() = default;
    ~Sqlite();

    Sqlite(const Sqlite&) = delete;
    Sqlite& operator=(const Sqlite&) = delete;

    // 打开数据库；readOnly=true 时以只读模式打开（词库库）。
    bool open(const QString& path, bool readOnly = false);
    void close();
    bool isOpen() const { return m_db != nullptr; }

    sqlite3* handle() const { return m_db; }

    // 执行一条或多条无返回结果的 SQL（建表、pragma 等）。
    bool exec(const QString& sql);

    // 便捷：创建预编译语句。
    Stmt prepare(const QString& sql) { return Stmt(m_db, sql); }

    // 事务
    void begin()    { exec(QStringLiteral("BEGIN")); }
    void commit()   { exec(QStringLiteral("COMMIT")); }
    void rollback() { exec(QStringLiteral("ROLLBACK")); }

    QString lastError() const;

private:
    sqlite3* m_db = nullptr;
};

} // namespace word
