#pragma once

// ---------------------------------------------------------------------------
// StudySession —— 学习会话（纯新词展示流 + 二选一强化）
//
// 行为（对应 need.md "学习功能"）：
//   * 本轮从未学新词中选 batchSize 个（计数项）
//   * 学习阶段只走本轮新词，不再随机穿插旧词
//   * 每张卡支持 认识 / 不认识 二选一判断
//   * 不认识 会在稍后再次出现，形成学习阶段延迟强化
//   * 全部展示完毕 → isFinished()；roundNewWords() 返回本轮新词供轮次巩固
//
// 可序列化为 JSON 以支持中断恢复。
// ---------------------------------------------------------------------------

#include <QString>
#include <QVector>

namespace word {

enum class StudyRating {
    Known = 0,
    Unknown = 1,
};

class StudySession {
public:
    StudySession() = default;

    // 开始新一轮。newWords：本轮新词（已按词频排好、长度=想学的数量）。
    void start(const QVector<int>& newWords);

    // 从/到 JSON（中断恢复）
    QString toJson() const;
    bool    fromJson(const QString& json);

    bool isFinished() const { return m_cursor >= m_queue.size(); }
    int  size() const { return m_queue.size(); }
    // 当前是否为队列最后一项：true 时本次判断完成后将进入轮次巩固。
    bool isLastCard() const { return !m_queue.isEmpty() && m_cursor == m_queue.size() - 1; }

    int  currentWordId() const;       // 当前项的词 id，-1 表示无
    bool currentIsReview() const;     // 当前项是否为强化回看项（不计数）

    void submitRating(StudyRating rating);

    // 进度
    int totalNew() const { return m_totalNew; }       // 本轮新词总数
    int learnedIndex() const;                          // 当前是第几个新词(1-based)
    const QVector<int>& roundNewWords() const { return m_roundNewWords; }
    bool currentCounted() const;
    bool currentNeedsReview() const;
    bool hasPendingReinforce() const { return m_pendingReinforce > 0; }
    int currentReinforceLevel() const;

private:
    struct Item {
        int wordId = 0;
        bool counted = false;
        int reinforceLevel = 0;
    };

    QVector<Item> m_queue;
    int           m_cursor = 0;
    int           m_totalNew = 0;
    QVector<int>  m_roundNewWords;
    int           m_pendingReinforce = 0;
};

} // namespace word
