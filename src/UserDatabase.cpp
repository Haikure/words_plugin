#include "UserDatabase.h"

#include <QDate>

namespace word {

bool UserDatabase::open(const QString& path) {
    if (!m_db.open(path, /*readOnly*/ false)) return false;
    ensureSchema();
    return true;
}

void UserDatabase::ensureSchema() {
    m_db.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS settings("
        "  key TEXT PRIMARY KEY, value TEXT);"

        "CREATE TABLE IF NOT EXISTS word_state("
        "  dict_id TEXT NOT NULL, word_id INTEGER NOT NULL,"
        "  status INTEGER DEFAULT 0,"
        "  ebbinghaus_stage INTEGER DEFAULT 0,"
        "  due_at INTEGER DEFAULT 0,"
        "  streak_correct INTEGER DEFAULT 0,"
        "  error_flag INTEGER DEFAULT 0,"
        "  pending_repeat INTEGER DEFAULT 0,"
        "  total_attempts INTEGER DEFAULT 0,"
        "  total_correct INTEGER DEFAULT 0,"
        "  first_learned_at INTEGER DEFAULT 0,"
        "  last_review_at INTEGER DEFAULT 0,"
        "  PRIMARY KEY(dict_id, word_id));"

        "CREATE INDEX IF NOT EXISTS idx_state_due "
        "  ON word_state(dict_id, status, due_at);"

        "CREATE TABLE IF NOT EXISTS session_state("
        "  id INTEGER PRIMARY KEY CHECK(id=1),"
        "  dict_id TEXT, mode INTEGER DEFAULT 0,"
        "  queue_json TEXT, cursor INTEGER DEFAULT 0,"
        "  batch_size INTEGER DEFAULT 0, updated_at INTEGER DEFAULT 0);"

        "CREATE TABLE IF NOT EXISTS daily_stat("
        "  day TEXT PRIMARY KEY,"
        "  learned INTEGER DEFAULT 0, reviewed INTEGER DEFAULT 0);"

        // 当天复习过的单词集合（按 天+词库+词 去重）——「今日复习」按词计数的依据。
        "CREATE TABLE IF NOT EXISTS daily_review_word("
        "  day TEXT NOT NULL, dict_id TEXT NOT NULL, word_id INTEGER NOT NULL,"
        "  PRIMARY KEY(day, dict_id, word_id));"));

    // 旧版本首次学习后会停在 status=1，导致首页「待复习」不增加。
    // 当前逻辑中已学词应立即进入复习池。
    m_db.exec(QStringLiteral(
        "UPDATE word_state SET status=2 WHERE status=1"));
}

// ---------------------------- 设置 ----------------------------

QString UserDatabase::getSetting(const QString& key, const QString& defValue) {
    Stmt stmt = m_db.prepare(QStringLiteral("SELECT value FROM settings WHERE key=?"));
    stmt.bind(1, key);
    if (stmt.step()) return stmt.columnText(0);
    return defValue;
}

void UserDatabase::setSetting(const QString& key, const QString& value) {
    Stmt stmt = m_db.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO settings(key,value) VALUES(?,?)"));
    stmt.bind(1, key);
    stmt.bind(2, value);
    stmt.step();
}

int UserDatabase::getSettingInt(const QString& key, int defValue) {
    const QString v = getSetting(key);
    bool ok = false;
    const int n = v.toInt(&ok);
    return ok ? n : defValue;
}

void UserDatabase::setSettingInt(const QString& key, int value) {
    setSetting(key, QString::number(value));
}

// ------------------------- 单词状态 -------------------------

WordState UserDatabase::wordState(const QString& dictId, int wordId) {
    WordState st;
    st.wordId = wordId;
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT status,ebbinghaus_stage,due_at,streak_correct,error_flag,"
        "pending_repeat,total_attempts,total_correct,first_learned_at,last_review_at "
        "FROM word_state WHERE dict_id=? AND word_id=?"));
    stmt.bind(1, dictId);
    stmt.bind(2, wordId);
    if (stmt.step()) {
        st.status          = stmt.columnInt(0);
        st.ebbinghausStage = stmt.columnInt(1);
        st.dueAt           = stmt.columnInt64(2);
        st.streakCorrect   = stmt.columnInt(3);
        st.errorFlag       = stmt.columnInt(4);
        st.pendingRepeat   = stmt.columnInt(5);
        st.totalAttempts   = stmt.columnInt(6);
        st.totalCorrect    = stmt.columnInt(7);
        st.firstLearnedAt  = stmt.columnInt64(8);
        st.lastReviewAt    = stmt.columnInt64(9);
    }
    return st;
}

void UserDatabase::saveWordState(const QString& dictId, const WordState& st) {
    Stmt stmt = m_db.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO word_state("
        "dict_id,word_id,status,ebbinghaus_stage,due_at,streak_correct,error_flag,"
        "pending_repeat,total_attempts,total_correct,first_learned_at,last_review_at)"
        " VALUES(?,?,?,?,?,?,?,?,?,?,?,?)"));
    stmt.bind(1, dictId);
    stmt.bind(2, st.wordId);
    stmt.bind(3, st.status);
    stmt.bind(4, st.ebbinghausStage);
    stmt.bind(5, st.dueAt);
    stmt.bind(6, st.streakCorrect);
    stmt.bind(7, st.errorFlag);
    stmt.bind(8, st.pendingRepeat);
    stmt.bind(9, st.totalAttempts);
    stmt.bind(10, st.totalCorrect);
    stmt.bind(11, st.firstLearnedAt);
    stmt.bind(12, st.lastReviewAt);
    stmt.step();
}

QVector<int> UserDatabase::unlearnedIds(const QString& dictId) {
    // 注意：word_state 中没有记录的词默认视为未学。
    // 真正的「未学候选」= 词库全集 - 已记录为 status>=1 的词，
    // 由上层（StudySession）用词库全集对照已学集合求差，这里只返回已学集合更高效。
    // 为简化，这里直接返回 word_state 中 status=0 的显式记录（一般为空）。
    QVector<int> ids;
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT word_id FROM word_state WHERE dict_id=? AND status=0"));
    stmt.bind(1, dictId);
    while (stmt.step()) ids.append(stmt.columnInt(0));
    return ids;
}

QVector<int> UserDatabase::learnedIds(const QString& dictId) {
    QVector<int> ids;
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT word_id FROM word_state WHERE dict_id=? AND status>=1"));
    stmt.bind(1, dictId);
    while (stmt.step()) ids.append(stmt.columnInt(0));
    return ids;
}

QVector<int> UserDatabase::reviewPoolIds(const QString& dictId, bool onlyDue, qint64 now) {
    QVector<int> ids;
    QString sql = QStringLiteral(
        "SELECT word_id FROM word_state WHERE dict_id=? AND status=2");
    if (onlyDue) sql += QStringLiteral(" AND due_at<=?");
    Stmt stmt = m_db.prepare(sql);
    stmt.bind(1, dictId);
    if (onlyDue) stmt.bind(2, now);
    while (stmt.step()) ids.append(stmt.columnInt(0));
    return ids;
}

QVector<int> UserDatabase::pendingFirstReviewIds(const QString& dictId) {
    QVector<int> ids;
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT word_id FROM word_state "
        "WHERE dict_id=? AND status=2 AND first_learned_at>0 AND last_review_at=0"));
    stmt.bind(1, dictId);
    while (stmt.step()) ids.append(stmt.columnInt(0));
    return ids;
}

int UserDatabase::learnedCount(const QString& dictId) {
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT count(*) FROM word_state WHERE dict_id=? AND status>=1"));
    stmt.bind(1, dictId);
    return stmt.step() ? stmt.columnInt(0) : 0;
}

int UserDatabase::reviewPoolCount(const QString& dictId) {
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT count(*) FROM word_state WHERE dict_id=? AND status=2"));
    stmt.bind(1, dictId);
    return stmt.step() ? stmt.columnInt(0) : 0;
}

int UserDatabase::dueCount(const QString& dictId, qint64 now) {
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT count(*) FROM word_state WHERE dict_id=? AND status=2 AND due_at<=?"));
    stmt.bind(1, dictId);
    stmt.bind(2, now);
    return stmt.step() ? stmt.columnInt(0) : 0;
}

// ---------------------- 每日统计 / 打卡 ----------------------

void UserDatabase::addTodayLearned(const QString& day, int delta) {
    m_db.exec(QStringLiteral(
        "INSERT OR IGNORE INTO daily_stat(day,learned,reviewed) VALUES('%1',0,0)")
        .arg(day));
    Stmt stmt = m_db.prepare(QStringLiteral(
        "UPDATE daily_stat SET learned=learned+? WHERE day=?"));
    stmt.bind(1, delta);
    stmt.bind(2, day);
    stmt.step();
}

void UserDatabase::recordReviewedWord(const QString& day, const QString& dictId, int wordId) {
    // 同一天内重复复习同一个词只计一次（按词计，而非按次数计）。
    Stmt stmt = m_db.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO daily_review_word(day,dict_id,word_id) VALUES(?,?,?)"));
    stmt.bind(1, day);
    stmt.bind(2, dictId);
    stmt.bind(3, wordId);
    stmt.step();
}

int UserDatabase::learnedBetween(qint64 startSecs, qint64 endSecs) {
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT count(*) FROM word_state "
        "WHERE status>=1 AND first_learned_at>=? AND first_learned_at<?"));
    stmt.bind(1, startSecs);
    stmt.bind(2, endSecs);
    return stmt.step() ? stmt.columnInt(0) : 0;
}

int UserDatabase::todayLearned(const QString& day) {
    Stmt stmt = m_db.prepare(QStringLiteral("SELECT learned FROM daily_stat WHERE day=?"));
    stmt.bind(1, day);
    return stmt.step() ? stmt.columnInt(0) : 0;
}

int UserDatabase::todayReviewed(const QString& day) {
    // 当天复习过的不同单词数（去重）。
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT count(*) FROM daily_review_word WHERE day=?"));
    stmt.bind(1, day);
    return stmt.step() ? stmt.columnInt(0) : 0;
}

int UserDatabase::streakDays(const QString& endDay) {
    // 从 endDay 往前数：某天有学习(learned>0) 或 有复习记录 即视为有效活动日。
    QDate day = QDate::fromString(endDay, QStringLiteral("yyyy-MM-dd"));
    if (!day.isValid()) return 0;

    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT 1 WHERE EXISTS(SELECT 1 FROM daily_stat WHERE day=? AND learned>0)"
        " OR EXISTS(SELECT 1 FROM daily_review_word WHERE day=?)"));
    int streak = 0;
    while (true) {
        const QString d = day.toString(QStringLiteral("yyyy-MM-dd"));
        stmt.reset();
        stmt.bind(1, d);
        stmt.bind(2, d);
        if (stmt.step()) {
            ++streak;
            day = day.addDays(-1);
        } else {
            break;
        }
    }
    return streak;
}

// ------------------------- 会话断点 -------------------------

SessionSnapshot UserDatabase::loadSession() {
    SessionSnapshot snap;
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT dict_id,mode,queue_json,cursor,batch_size,updated_at "
        "FROM session_state WHERE id=1"));
    if (stmt.step()) {
        snap.dictId    = stmt.columnText(0);
        snap.mode      = stmt.columnInt(1);
        snap.queueJson = stmt.columnText(2);
        snap.cursor    = stmt.columnInt(3);
        snap.batchSize = stmt.columnInt(4);
        snap.updatedAt = stmt.columnInt64(5);
    }
    return snap;
}

void UserDatabase::saveSession(const SessionSnapshot& snap) {
    Stmt stmt = m_db.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO session_state("
        "id,dict_id,mode,queue_json,cursor,batch_size,updated_at)"
        " VALUES(1,?,?,?,?,?,?)"));
    stmt.bind(1, snap.dictId);
    stmt.bind(2, snap.mode);
    stmt.bind(3, snap.queueJson);
    stmt.bind(4, snap.cursor);
    stmt.bind(5, snap.batchSize);
    stmt.bind(6, snap.updatedAt);
    stmt.step();
}

void UserDatabase::clearSession() {
    m_db.exec(QStringLiteral("DELETE FROM session_state WHERE id=1"));
}

} // namespace word
