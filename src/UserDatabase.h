#pragma once

// ---------------------------------------------------------------------------
// UserDatabase —— 用户数据（读写）访问层
//
// 职责：
//   * 建表并维护 settings / word_state / session_state / daily_stat
//   * 学习进度与复习状态（按 dict_id 区分不同词库）
//   * 每日学习/复习统计与连续打卡天数
//   * 会话断点（session_state 单行）持久化与读取
// 全局一个 user.db；切换词库不丢状态（word_state 以 dict_id 区分）。
// ---------------------------------------------------------------------------

#include "Sqlite.h"

#include <QString>
#include <QVector>

namespace word {

// 单词的学习/复习状态（word_state 表一行）
struct WordState {
    int     wordId = 0;
    int     status = 0;            // 0未学 / 1旧版学习中 / 2已学(进复习池)
    int     ebbinghausStage = 0;   // 0..5 → 1/2/4/7/15/30 天
    qint64  dueAt = 0;             // 下次复习到期(unix秒)
    int     streakCorrect = 0;     // 连续正确次数
    int     errorFlag = 0;         // 高错误标记
    int     pendingRepeat = 0;     // 本会话待再现次数
    int     totalAttempts = 0;
    int     totalCorrect = 0;
    qint64  firstLearnedAt = 0;
    qint64  lastReviewAt = 0;
};

// 会话断点（session_state 单行）
struct SessionSnapshot {
    int     mode = 0;          // 0无 / 1学习 / 2复习 / 3轮次巩固 / 5随机复习
    QString dictId;
    QString queueJson;         // 队列与进度的 JSON
    int     cursor = 0;
    int     batchSize = 0;
    qint64  updatedAt = 0;

    bool hasUnfinished() const { return mode != 0; }
};

class UserDatabase {
public:
    UserDatabase() = default;

    // 打开（不存在则创建）用户库并建表。
    bool open(const QString& path);
    bool isOpen() const { return m_db.isOpen(); }

    // ---- 设置（kv）----
    QString getSetting(const QString& key, const QString& defValue = QString());
    void    setSetting(const QString& key, const QString& value);
    int     getSettingInt(const QString& key, int defValue);
    void    setSettingInt(const QString& key, int value);

    // ---- 单词状态 ----
    WordState wordState(const QString& dictId, int wordId);
    void      saveWordState(const QString& dictId, const WordState& st);

    // 取指定词库中 status=0 的「未学」词 id（外部再按词频排序选取）。
    QVector<int> unlearnedIds(const QString& dictId);
    // 取已学（status>=1）的词 id，用于学习时随机插入。
    QVector<int> learnedIds(const QString& dictId);
    // 取复习池：status=2 的词 id；onlyDue=true 时仅取已到期(due_at<=now)。
    QVector<int> reviewPoolIds(const QString& dictId, bool onlyDue, qint64 now);
    // 取已学习但尚未完成首次复习的词（优先用于承接上次学习后的复习）。
    QVector<int> pendingFirstReviewIds(const QString& dictId);

    int learnedCount(const QString& dictId);    // status>=1 的数量
    int reviewPoolCount(const QString& dictId); // status=2 的数量
    int dueCount(const QString& dictId, qint64 now); // status=2 且到期的数量

    // ---- 每日统计 / 打卡 ----
    void addTodayLearned(const QString& day, int delta);
    // 记录某天复习过某词；按「天×词库×词」去重，重复复习同一词不重复计数。
    void recordReviewedWord(const QString& day, const QString& dictId, int wordId);
    int  learnedBetween(qint64 startSecs, qint64 endSecs);
    int  todayLearned(const QString& day);
    int  todayReviewed(const QString& day);  // 当天复习过的不同单词数
    // 连续打卡天数：从 endDay 往前数连续有学习/复习记录的天数。
    int  streakDays(const QString& endDay);

    // ---- 会话断点 ----
    SessionSnapshot loadSession();
    void            saveSession(const SessionSnapshot& snap);
    void            clearSession();

private:
    void ensureSchema();

    Sqlite m_db;
};

} // namespace word
