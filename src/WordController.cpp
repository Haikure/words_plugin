#include "WordController.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QRandomGenerator>
#include <QSet>
#include <QStringList>
#include <QTime>
#include <QtGlobal>

namespace word {

// 会话模式常量
enum Mode { ModeIdle = 0, ModeStudy = 1, ModeReview = 2, ModeConsolidate = 3, ModeSummary = 4, ModeRandomReview = 5 };

// 把 ECDICT 字面 "\n" 转为真换行
static QString norm(const QString& s) {
    QString o = s;
    o.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    return o;
}

// Fisher-Yates 洗牌
static void shuffle(QVector<int>& v) {
    auto* rng = QRandomGenerator::global();
    for (int i = v.size() - 1; i > 0; --i) {
        const int j = rng->bounded(i + 1);
        const int tmp = v[i];
        v[i] = v[j];
        v[j] = tmp;
    }
}

static QStringList uniqueOptions(const QStringList& raw, const QString& correct, int limit) {
    QStringList out;
    QSet<QString> seen;
    seen.insert(correct.trimmed());
    for (const QString& item : raw) {
        const QString key = item.trimmed();
        if (key.isEmpty() || seen.contains(key)) continue;
        seen.insert(key);
        out.append(item);
        if (out.size() >= limit) break;
    }
    return out;
}

WordController* WordController::instance() {
    static WordController s;
    return &s;
}

WordController::WordController(QObject* parent) : QObject(parent) {}

qint64 WordController::nowSecs() {
    return QDateTime::currentSecsSinceEpoch();
}

QString WordController::today() {
    return QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
}

static qint64 dayStartSecs(const QDate& day) {
    return QDateTime(day, QTime(0, 0)).toSecsSinceEpoch();
}

// ----------------------------- 初始化 -----------------------------

void WordController::setBaseDir(const QString& dir) {
    m_baseDir  = dir;
    m_dictsDir = dir + QStringLiteral("/dicts");

    QDir().mkpath(dir + QStringLiteral("/userdata"));
    m_user.open(dir + QStringLiteral("/userdata/user.db"));

    m_dict.scan(m_dictsDir);

    m_batchSize = m_user.getSettingInt(QStringLiteral("batch_size"), 10);

    QString cur = m_user.getSetting(QStringLiteral("current_dict"));
    if (cur.isEmpty() && !m_dict.dicts().isEmpty())
        cur = m_dict.dicts().first().dictId;

    bool opened = false;
    if (!cur.isEmpty())
        opened = m_dict.open(cur);
    if (!opened && !m_dict.dicts().isEmpty()) {
        cur = m_dict.dicts().first().dictId;
        opened = m_dict.open(cur);
        if (opened)
            m_user.setSetting(QStringLiteral("current_dict"), cur);
    }

    const SessionSnapshot snap = m_user.loadSession();
    m_hasUnfinished = snap.hasUnfinished();

    emit dictChanged();
    emit statsChanged();
    emit batchSizeChanged();
    emit unfinishedChanged();
}

// ----------------------------- 属性 -----------------------------

int WordController::learnedCount() const {
    return const_cast<UserDatabase&>(m_user).learnedCount(m_dict.currentDictId());
}
int WordController::todayLearned() const {
    const QDate day = QDate::currentDate();
    return const_cast<UserDatabase&>(m_user).learnedBetween(dayStartSecs(day),
                                                            dayStartSecs(day.addDays(1)));
}
int WordController::todayReviewed() const {
    return const_cast<UserDatabase&>(m_user).todayReviewed(today());
}
int WordController::streakDays() const {
    return const_cast<UserDatabase&>(m_user).streakDays(today());
}
int WordController::dueCount() const {
    return const_cast<UserDatabase&>(m_user).dueCount(m_dict.currentDictId(), nowSecs());
}

void WordController::setSessionMode(int mode) {
    if (m_sessionMode == mode) return;
    m_sessionMode = mode;
    emit sessionModeChanged();
}

// ----------------------------- 词库 -----------------------------

QVariantList WordController::dictList() {
    QVariantList list;
    const QString curId = m_dict.currentDictId();
    for (const DictInfo& info : m_dict.dicts()) {
        const int learned = m_user.learnedCount(info.dictId);
        QVariantMap m;
        m[QStringLiteral("dictId")]       = info.dictId;
        m[QStringLiteral("name")]         = info.name;
        m[QStringLiteral("description")]  = info.description;
        m[QStringLiteral("wordCount")]    = info.wordCount;
        m[QStringLiteral("learnedCount")] = learned;
        m[QStringLiteral("progress")]     =
            info.wordCount > 0 ? double(learned) / info.wordCount : 0.0;
        m[QStringLiteral("current")]      = (info.dictId == curId);
        list.append(m);
    }
    return list;
}

bool WordController::selectDict(const QString& dictId) {
    if (m_sessionMode != ModeIdle) persistSession();  // 切库前保存当前会话状态

    if (!m_dict.open(dictId)) return false;
    m_user.setSetting(QStringLiteral("current_dict"), dictId);
    setSessionMode(ModeIdle);

    emit dictChanged();
    emit statsChanged();
    return true;
}

void WordController::setBatchSize(int n) {
    if (n < 5) n = 5;
    if (n > 30) n = 30;
    if (m_batchSize == n) return;
    m_batchSize = n;
    m_user.setSettingInt(QStringLiteral("batch_size"), n);
    emit batchSizeChanged();
}

// ----------------------------- 学习 -----------------------------

bool WordController::startStudy() {
    if (!m_dict.isOpen()) return false;

    // 已学集合（status>=1）
    const QVector<int> learnedVec = m_user.learnedIds(m_dict.currentDictId());
    QSet<int> learnedSet;
    for (int id : learnedVec) learnedSet.insert(id);

    // 从所有未学词中随机抽取本轮新词，避免每次都偏向词频排序靠前的词。
    const QVector<int> all = m_dict.allWordIdsByFreq();
    QVector<int> picks;
    for (int id : all) {
        if (!learnedSet.contains(id)) picks.append(id);
    }

    shuffle(picks);
    if (picks.size() > m_batchSize) picks.resize(m_batchSize);
    if (picks.isEmpty()) return false;  // 没有新词可学

    m_study.start(picks);
    setSessionMode(ModeStudy);
    refreshStudyCard();
    persistSession();
    return true;
}

void WordController::refreshStudyCard() {
    const int id = m_study.currentWordId();
    if (id < 0) { m_currentCard = QVariantMap(); emit currentCardChanged(); return; }

    const WordEntry e = m_dict.wordById(id);
    m_currentCard = e.toVariantMap();
    m_currentCard[QStringLiteral("isReview")] = m_study.currentIsReview();
    m_currentCard[QStringLiteral("index")]    = m_study.learnedIndex();
    m_currentCard[QStringLiteral("total")]    = m_study.totalNew();
    m_currentCard[QStringLiteral("isLast")]   = m_study.isLastCard();
    m_currentCard[QStringLiteral("needsReview")] = m_study.currentNeedsReview();
    m_currentCard[QStringLiteral("reinforceLevel")] = m_study.currentReinforceLevel();
    emit currentCardChanged();
}

void WordController::rateStudy(int rating) {
    if (m_sessionMode != ModeStudy) return;

    const int id = m_study.currentWordId();
    const bool counted = m_study.currentCounted();
    const StudyRating studyRating = (rating <= 0) ? StudyRating::Known
                                                   : StudyRating::Unknown;

    if (id > 0 && counted) {
        WordState st = m_user.wordState(m_dict.currentDictId(), id);
        if (st.status == 0) {                 // 首次学习此新词
            const qint64 now = nowSecs();
            st.status = 2;
            st.firstLearnedAt = now;
            st.dueAt = now;
            m_user.saveWordState(m_dict.currentDictId(), st);
            m_user.addTodayLearned(today(), 1);
        }
    }

    m_study.submitRating(studyRating);

    if (m_study.isFinished()) {
        finishStudyEnterConsolidate();
    } else {
        refreshStudyCard();
        persistSession();
        emit statsChanged();
    }
}

void WordController::finishStudyEnterConsolidate() {
    QVector<int> round = m_study.roundNewWords();
    if (round.isEmpty()) { discardBrokenSession(); return; }

    m_review.start(round);
    setSessionMode(ModeConsolidate);
    refreshReviewQuestion();
    persistSession();
    emit statsChanged();
}

// ----------------------------- 复习 -----------------------------

bool WordController::startReview() {
    if (!m_dict.isOpen()) return false;
    const QString dictId = m_dict.currentDictId();
    const qint64 now = nowSecs();

    // 优先承接上次学习后尚未完成首次复习的新词。
    QVector<int> pool = m_user.pendingFirstReviewIds(dictId);
    // 其次取已到期的复习词。
    if (pool.isEmpty())
        pool = m_user.reviewPoolIds(dictId, /*onlyDue*/ true, now);
    // 没有待复习词时，允许从已学词里随机抽一批做自由复习；自由复习不写入长期复习统计。
    bool randomReview = false;
    if (pool.isEmpty()) {
        pool = m_user.learnedIds(dictId);
        randomReview = true;
    }
    if (pool.isEmpty()) return false;

    shuffle(pool);
    if (pool.size() > m_batchSize) pool.resize(m_batchSize);

    m_review.start(pool);
    setSessionMode(randomReview ? ModeRandomReview : ModeReview);
    refreshReviewQuestion();
    persistSession();
    return true;
}

void WordController::refreshReviewQuestion() {
    const int id = m_review.currentWordId();
    if (id < 0) { finishReview(); return; }

    const WordEntry e = m_dict.wordById(id);
    const bool en2cn = m_review.askEnToCn();

    QString prompt, promptPhonetic, correct;
    QStringList options;
    if (en2cn) {
        // 看英文（带音标）选中文
        prompt = e.word;
        promptPhonetic = e.phonetic;
        correct = norm(e.translation);
        options = uniqueOptions(m_dict.randomTranslations(8, id), correct, 3);
    } else {
        // 看中文选英文
        prompt = norm(e.translation);
        correct = e.word;
        options = uniqueOptions(m_dict.randomWords(8, id), correct, 3);
    }

    // 正确项插入随机位置
    int answerIndex = QRandomGenerator::global()->bounded(options.size() + 1);
    options.insert(answerIndex, correct);
    m_lastCorrectIndex = answerIndex;

    QVariantMap q;
    q[QStringLiteral("wordId")]         = id;
    q[QStringLiteral("askEnToCn")]      = en2cn;
    q[QStringLiteral("prompt")]         = prompt;
    q[QStringLiteral("promptPhonetic")] = promptPhonetic;
    q[QStringLiteral("options")]        = options;
    q[QStringLiteral("answerIndex")]    = answerIndex;
    q[QStringLiteral("index")]          = m_review.displayIndex();
    q[QStringLiteral("total")]          = m_review.totalCount();
    q[QStringLiteral("isConsolidate")]  = (m_sessionMode == ModeConsolidate);
    q[QStringLiteral("isRandomReview")] = (m_sessionMode == ModeRandomReview);
    m_currentQuestion = q;
    emit currentQuestionChanged();
}

bool WordController::answerReview(int optionIndex) {
    if (m_sessionMode != ModeReview && m_sessionMode != ModeConsolidate && m_sessionMode != ModeRandomReview)
        return false;

    const int id = m_review.currentWordId();
    const bool correct = (optionIndex == m_lastCorrectIndex);

    if (id > 0 && m_sessionMode == ModeReview) {
        WordState st = m_user.wordState(m_dict.currentDictId(), id);
        if (m_review.currentIsCounted()) {
            ReviewSession::applyResult(st, correct, nowSecs());
            m_user.saveWordState(m_dict.currentDictId(), st);
            // 按「词×天」去重，重复复习不重复计数。
            m_user.recordReviewedWord(today(), m_dict.currentDictId(), id);
        }
    }

    m_review.submitAnswer(correct);  // 前进队列 + 当前会话内纠错重排
    persistSession();
    if (m_sessionMode != ModeRandomReview) emit statsChanged();
    return correct;
}

void WordController::advanceReview() {
    if (m_sessionMode != ModeReview && m_sessionMode != ModeConsolidate && m_sessionMode != ModeRandomReview)
        return;
    if (m_review.isFinished())
        finishReview();
    else
        refreshReviewQuestion();
}

void WordController::finishReview() {
    const bool wasConsolidate = (m_sessionMode == ModeConsolidate);
    const bool wasRandomReview = (m_sessionMode == ModeRandomReview);
    const int total = m_review.totalCount();
    const int correct = m_review.correctCount();
    const int wrong = m_review.wrongCount();

    m_user.clearSession();
    m_currentQuestion = QVariantMap();
    emit currentQuestionChanged();

    QVariantMap summary;
    summary[QStringLiteral("kind")] = wasConsolidate ? QStringLiteral("study")
                                                     : QStringLiteral("review");
    summary[QStringLiteral("title")] = wasConsolidate ? QStringLiteral("本轮完成")
                                      : (wasRandomReview ? QStringLiteral("随机复习完成")
                                                         : QStringLiteral("复习完成"));
    summary[QStringLiteral("total")] = total;
    summary[QStringLiteral("correct")] = correct;
    summary[QStringLiteral("wrong")] = wrong;
    summary[QStringLiteral("accuracy")] = total > 0 ? double(correct) / total : 0.0;
    summary[QStringLiteral("due")] = dueCount();
    m_currentSummary = summary;
    emit currentSummaryChanged();

    setSessionMode(ModeSummary);
    m_hasUnfinished = false;
    emit unfinishedChanged();
    emit statsChanged();
}

void WordController::discardBrokenSession() {
    m_user.clearSession();
    m_currentCard = QVariantMap();
    m_currentQuestion = QVariantMap();
    m_hasUnfinished = false;
    setSessionMode(ModeIdle);
    emit currentCardChanged();
    emit currentQuestionChanged();
    emit unfinishedChanged();
    emit statsChanged();
}

// ----------------------------- 中断恢复 -----------------------------

void WordController::persistSession() {
    SessionSnapshot snap;
    snap.dictId    = m_dict.currentDictId();
    snap.mode      = m_sessionMode;
    snap.batchSize = m_batchSize;
    snap.updatedAt = nowSecs();
    if (m_sessionMode == ModeStudy)
        snap.queueJson = m_study.toJson();
    else if (m_sessionMode == ModeReview || m_sessionMode == ModeConsolidate || m_sessionMode == ModeRandomReview)
        snap.queueJson = m_review.toJson();
    else
        return;  // 空闲不写

    m_user.saveSession(snap);
    m_hasUnfinished = true;
}

void WordController::resumeSession() {
    const SessionSnapshot snap = m_user.loadSession();
    if (!snap.hasUnfinished()) return;

    if (snap.dictId.isEmpty()) {
        discardBrokenSession();
        return;
    }

    if (m_dict.currentDictId() != snap.dictId) {
        if (!m_dict.open(snap.dictId)) {
            discardBrokenSession();
            return;
        }
        m_user.setSetting(QStringLiteral("current_dict"), snap.dictId);
    } else if (!m_dict.isOpen()) {
        if (!m_dict.open(snap.dictId)) {
            discardBrokenSession();
            return;
        }
    }
    if (snap.batchSize > 0) m_batchSize = snap.batchSize;

    if (snap.mode == ModeStudy) {
        if (!m_study.fromJson(snap.queueJson)) {
            discardBrokenSession();
            return;
        }
        if (m_study.isFinished()) {
            finishStudyEnterConsolidate();
            return;
        }
        setSessionMode(ModeStudy);
        refreshStudyCard();
    } else if (snap.mode == ModeReview || snap.mode == ModeConsolidate || snap.mode == ModeRandomReview) {
        if (!m_review.fromJson(snap.queueJson)) {
            discardBrokenSession();
            return;
        }
        if (m_review.isFinished()) {
            setSessionMode(snap.mode);
            finishReview();
            return;
        }
        setSessionMode(snap.mode);
        refreshReviewQuestion();
    } else {
        discardBrokenSession();
        return;
    }

    m_hasUnfinished = false;
    emit dictChanged();
    emit batchSizeChanged();
    emit unfinishedChanged();
}

void WordController::discardSession() {
    m_user.clearSession();
    m_hasUnfinished = false;
    setSessionMode(ModeIdle);
    emit unfinishedChanged();
}

void WordController::saveState() {
    if (m_sessionMode != ModeIdle) persistSession();
}

void WordController::goHome() {
    if (m_sessionMode != ModeIdle && m_sessionMode != ModeSummary) persistSession();
    if (m_sessionMode == ModeSummary) {
        m_currentSummary = QVariantMap();
        emit currentSummaryChanged();
    }
    setSessionMode(ModeIdle);
    const SessionSnapshot snap = m_user.loadSession();
    m_hasUnfinished = snap.hasUnfinished();
    emit unfinishedChanged();
    emit statsChanged();
}

} // namespace word
