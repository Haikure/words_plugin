#pragma once

// ---------------------------------------------------------------------------
// WordController —— 暴露给 QML 的门面单例
//
// 通过 attach_engine 注册为 QML 上下文属性 "wordController"。
// 聚合词库（只读）、用户库（读写）、学习会话、复习会话，向 QML 提供：
//   * 主页统计属性（今日学/复、打卡、进度、待复习数）
//   * 词库列表与切换
//   * 学习流（startStudy / rateStudy，自动转入轮次巩固）
//   * 复习流（startReview / answerReview / advanceReview）
//   * 中断恢复（hasUnfinishedSession / resumeSession / discardSession / saveState）
// ---------------------------------------------------------------------------

#include "DictDatabase.h"
#include "ReviewSession.h"
#include "StudySession.h"
#include "UserDatabase.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace word {

class WordController : public QObject {
    Q_OBJECT

    // —— 会话模式：0空闲 / 1学习 / 2复习 / 3轮次巩固 / 4总结 ——
    Q_PROPERTY(int sessionMode READ sessionMode NOTIFY sessionModeChanged)

    // —— 当前词库 ——
    Q_PROPERTY(QString currentDictName READ currentDictName NOTIFY dictChanged)
    Q_PROPERTY(int totalWords READ totalWords NOTIFY dictChanged)
    Q_PROPERTY(int learnedCount READ learnedCount NOTIFY statsChanged)

    // —— 主页统计 ——
    Q_PROPERTY(int todayLearned READ todayLearned NOTIFY statsChanged)
    Q_PROPERTY(int todayReviewed READ todayReviewed NOTIFY statsChanged)
    Q_PROPERTY(int streakDays READ streakDays NOTIFY statsChanged)
    Q_PROPERTY(int dueCount READ dueCount NOTIFY statsChanged)

    // —— 设置 ——
    Q_PROPERTY(int batchSize READ batchSize WRITE setBatchSize NOTIFY batchSizeChanged)

    // —— 中断恢复 ——
    Q_PROPERTY(bool hasUnfinishedSession READ hasUnfinishedSession NOTIFY unfinishedChanged)

    // —— 当前学习卡 / 复习题（map）——
    Q_PROPERTY(QVariantMap currentCard READ currentCard NOTIFY currentCardChanged)
    Q_PROPERTY(QVariantMap currentQuestion READ currentQuestion NOTIFY currentQuestionChanged)
    Q_PROPERTY(QVariantMap currentSummary READ currentSummary NOTIFY currentSummaryChanged)

public:
    static WordController* instance();

    // 由 plugin.cpp 在 init 时调用：设置插件根目录（.so 所在目录），随即完成初始化。
    void setBaseDir(const QString& dir);

    // 属性读取
    int     sessionMode() const { return m_sessionMode; }
    QString currentDictName() const { return m_dict.currentInfo().name; }
    int     totalWords() const { return m_dict.totalWords(); }
    int     learnedCount() const;
    int     todayLearned() const;
    int     todayReviewed() const;
    int     streakDays() const;
    int     dueCount() const;
    int     batchSize() const { return m_batchSize; }
    bool    hasUnfinishedSession() const { return m_hasUnfinished; }
    QVariantMap currentCard() const { return m_currentCard; }
    QVariantMap currentQuestion() const { return m_currentQuestion; }
    QVariantMap currentSummary() const { return m_currentSummary; }

    // —— 词库 ——
    Q_INVOKABLE QVariantList dictList();
    Q_INVOKABLE bool selectDict(const QString& dictId);

    // —— 设置 ——
    Q_INVOKABLE void setBatchSize(int n);

    // —— 学习 ——
    Q_INVOKABLE bool startStudy();          // 无新词可学返回 false
    Q_INVOKABLE void rateStudy(int rating); // 0认识 / 1不认识

    // —— 复习 ——
    Q_INVOKABLE bool startReview();         // 无到期/首次复习词返回 false
    Q_INVOKABLE bool answerReview(int optionIndex); // 返回是否答对（不前进题目）
    Q_INVOKABLE int  lastCorrectIndex() const { return m_lastCorrectIndex; }
    Q_INVOKABLE void advanceReview();       // 反馈展示后调用：前进到下一题/结束

    // —— 中断恢复 ——
    Q_INVOKABLE void resumeSession();
    Q_INVOKABLE void discardSession();
    Q_INVOKABLE void saveState();           // 退出/隐藏时持久化当前会话
    Q_INVOKABLE void goHome();              // 主动结束当前会话并保存

signals:
    void sessionModeChanged();
    void dictChanged();
    void statsChanged();
    void batchSizeChanged();
    void unfinishedChanged();
    void currentCardChanged();
    void currentQuestionChanged();
    void currentSummaryChanged();

private:
    explicit WordController(QObject* parent = nullptr);

    void setSessionMode(int mode);
    void refreshStudyCard();                 // 刷新 m_currentCard
    void refreshReviewQuestion();            // 刷新 m_currentQuestion（生成四选一）
    void finishStudyEnterConsolidate();      // 学习完成 → 轮次巩固
    void finishReview();                     // 复习/巩固结束 → 总结页
    void discardBrokenSession();             // 断点不可恢复时清理并回主页
    void persistSession();                   // 把活动会话写入 session_state

    static qint64 nowSecs();
    static QString today();

    QString          m_baseDir;
    QString          m_dictsDir;
    DictDatabase     m_dict;
    UserDatabase     m_user;
    StudySession     m_study;
    ReviewSession    m_review;

    int  m_sessionMode = 0;
    int  m_batchSize = 10;
    bool m_hasUnfinished = false;

    QVariantMap m_currentCard;
    QVariantMap m_currentQuestion;
    QVariantMap m_currentSummary;
    int         m_lastCorrectIndex = -1;
};

} // namespace word
