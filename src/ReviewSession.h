#pragma once

// ---------------------------------------------------------------------------
// ReviewSession —— 复习会话（四选一），同时用于「轮次巩固」与「独立复习」
//
// 行为：
//   * 计数项 = 本次要复习的词；看英文选中文 / 看中文选英文 随机混合
//   * 答错 → 在当前会话内做延迟随机纠错重现（不计数）
//   * 仅计数题写回长期复习状态：艾宾浩斯 1/2/4/7/15/30 天，
//     连对≥3 清除高错标记，答错回退间隔
//
// 选项内容由上层（WordController + DictDatabase）生成，本类只管队列与方向。
// 可序列化为 JSON 以支持中断恢复。
// ---------------------------------------------------------------------------

#include "UserDatabase.h"

#include <QHash>
#include <QString>
#include <QVector>

namespace word {

class ReviewSession {
public:
    ReviewSession() = default;

    // 开始复习：words 为参与复习的词（计数项），按给定顺序。
    void start(const QVector<int>& words);

    QString toJson() const;
    bool    fromJson(const QString& json);

    bool isFinished() const { return m_cursor >= m_queue.size(); }

    int  currentWordId() const;     // 当前题目的词 id，-1 表示无
    bool currentIsCounted() const;  // 当前项是否计数（false=纠错重现）
    bool askEnToCn() const;         // 当前题目方向：true=看英文选中文

    // 提交答案后推进队列；correct=false 时插回延迟纠错题。
    void submitAnswer(bool correct);

    int answeredCount() const;      // 已完成的计数项数
    int totalCount() const { return m_totalCount; }
    int correctCount() const { return m_correctCount; }
    int wrongCount() const { return m_answeredCount - m_correctCount; }
    int displayIndex() const;

    // 根据计数题的对错更新单词的长期复习状态（跨会话调度）。
    static void applyResult(WordState& st, bool correct, qint64 now);

private:
    struct Item { int wordId; bool counted; };

    void insertDelayedRetry(int wordId, int minGap, int maxGap, int minSameWordGap);
    bool hasPendingRetryForWord(int wordId) const;
    bool canInsertRetryAt(int wordId, int pos, int minSameWordGap, bool avoidRetryNeighbor) const;

    QVector<Item>      m_queue;
    int                m_cursor = 0;
    int                m_totalCount = 0;
    int                m_answeredCount = 0;
    int                m_correctCount = 0;
    QHash<int, int>    m_sessionError;  // 会话内每词错误次数
    QHash<int, bool>   m_dir;           // 每词题目方向（true=看英文选中文）
};

} // namespace word
