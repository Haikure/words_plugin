#include "ReviewSession.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>

namespace word {

// 艾宾浩斯间隔（天）：stage 0..5
static qint64 stageIntervalSecs(int stage) {
    static const int kDays[] = {1, 2, 4, 7, 15, 30};
    if (stage < 0) stage = 0;
    if (stage > 5) stage = 5;
    return static_cast<qint64>(kDays[stage]) * 24 * 3600;
}

// 防止极端情况下重现项无限堆积
static const int kMaxQueueLen = 512;

void ReviewSession::start(const QVector<int>& words) {
    m_queue.clear();
    m_sessionError.clear();
    m_dir.clear();
    m_cursor = 0;
    m_totalCount = words.size();
    m_answeredCount = 0;
    m_correctCount = 0;

    auto* rng = QRandomGenerator::global();
    for (int id : words) {
        m_queue.append({id, true});
        m_dir[id] = (rng->bounded(2) == 0); // 随机方向
    }
}

QString ReviewSession::toJson() const {
    QJsonArray queue;
    for (const Item& it : m_queue) {
        QJsonObject o;
        o[QStringLiteral("w")] = it.wordId;
        o[QStringLiteral("c")] = it.counted;
        queue.append(o);
    }
    QJsonObject errs;
    for (auto it = m_sessionError.constBegin(); it != m_sessionError.constEnd(); ++it)
        errs[QString::number(it.key())] = it.value();
    QJsonObject dirs;
    for (auto it = m_dir.constBegin(); it != m_dir.constEnd(); ++it)
        dirs[QString::number(it.key())] = it.value();

    QJsonObject root;
    root[QStringLiteral("queue")]  = queue;
    root[QStringLiteral("cursor")] = m_cursor;
    root[QStringLiteral("total")]  = m_totalCount;
    root[QStringLiteral("answered")] = m_answeredCount;
    root[QStringLiteral("correct")] = m_correctCount;
    root[QStringLiteral("errs")]   = errs;
    root[QStringLiteral("dirs")]   = dirs;
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool ReviewSession::fromJson(const QString& json) {
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) return false;
    const QJsonObject root = doc.object();

    m_queue.clear();
    const QJsonArray queue = root[QStringLiteral("queue")].toArray();
    for (const QJsonValue& v : queue) {
        const QJsonObject o = v.toObject();
        m_queue.append({o[QStringLiteral("w")].toInt(), o[QStringLiteral("c")].toBool()});
    }
    m_cursor     = root[QStringLiteral("cursor")].toInt();
    m_totalCount = root[QStringLiteral("total")].toInt();
    m_answeredCount = root.contains(QStringLiteral("answered"))
        ? root[QStringLiteral("answered")].toInt()
        : answeredCount();
    m_correctCount = root[QStringLiteral("correct")].toInt();

    m_sessionError.clear();
    const QJsonObject errs = root[QStringLiteral("errs")].toObject();
    for (auto it = errs.constBegin(); it != errs.constEnd(); ++it)
        m_sessionError[it.key().toInt()] = it.value().toInt();

    m_dir.clear();
    const QJsonObject dirs = root[QStringLiteral("dirs")].toObject();
    for (auto it = dirs.constBegin(); it != dirs.constEnd(); ++it)
        m_dir[it.key().toInt()] = it.value().toBool();

    if (m_queue.isEmpty()) return false;
    if (m_cursor < 0 || m_cursor > m_queue.size()) return false;
    if (m_totalCount <= 0) return false;
    if (m_answeredCount < 0) m_answeredCount = 0;
    if (m_correctCount < 0) m_correctCount = 0;
    if (m_answeredCount > m_totalCount) m_answeredCount = m_totalCount;
    if (m_correctCount > m_answeredCount) m_correctCount = m_answeredCount;
    return true;
}

int ReviewSession::currentWordId() const {
    if (m_cursor < 0 || m_cursor >= m_queue.size()) return -1;
    return m_queue[m_cursor].wordId;
}

bool ReviewSession::currentIsCounted() const {
    if (m_cursor < 0 || m_cursor >= m_queue.size()) return false;
    return m_queue[m_cursor].counted;
}

bool ReviewSession::askEnToCn() const {
    const int id = currentWordId();
    return id > 0 ? m_dir.value(id, true) : true;
}

void ReviewSession::submitAnswer(bool correct) {
    const int id = currentWordId();
    if (id <= 0) return;

    const bool counted = currentIsCounted();
    if (counted) {
        ++m_answeredCount;
        if (correct) ++m_correctCount;
    }

    if (!correct) {
        const int cnt = ++m_sessionError[id];
        if (!hasPendingRetryForWord(id)) {
            const int minGap = (cnt == 1) ? 3 : 5;
            const int maxGap = (cnt == 1) ? 8 : 12;
            insertDelayedRetry(id, minGap, maxGap, 3);
        }
    }
    ++m_cursor;
}

void ReviewSession::insertDelayedRetry(int wordId, int minGap, int maxGap, int minSameWordGap) {
    if (m_queue.size() >= kMaxQueueLen) return;

    const int afterCurrent = m_cursor + 1;
    if (afterCurrent >= m_queue.size()) return;

    if (maxGap < minGap) maxGap = minGap;
    const int lower = qMin(afterCurrent + minGap, m_queue.size());
    const int upper = qMin(afterCurrent + maxGap, m_queue.size());

    QVector<int> positions;
    for (int pos = lower; pos <= upper; ++pos) {
        if (canInsertRetryAt(wordId, pos, minSameWordGap, true))
            positions.append(pos);
    }

    if (positions.isEmpty()) {
        for (int pos = lower; pos <= upper; ++pos) {
            if (canInsertRetryAt(wordId, pos, minSameWordGap, false))
                positions.append(pos);
        }
    }

    int pos = -1;
    if (!positions.isEmpty()) {
        pos = positions[QRandomGenerator::global()->bounded(positions.size())];
    } else if (m_queue.size() > afterCurrent &&
               canInsertRetryAt(wordId, m_queue.size(), minSameWordGap, false)) {
        pos = m_queue.size();
    }

    if (pos >= 0)
        m_queue.insert(pos, {wordId, false});
}

bool ReviewSession::hasPendingRetryForWord(int wordId) const {
    for (int i = m_cursor + 1; i < m_queue.size(); ++i) {
        if (m_queue[i].wordId == wordId && !m_queue[i].counted)
            return true;
    }
    return false;
}

bool ReviewSession::canInsertRetryAt(int wordId, int pos, int minSameWordGap, bool avoidRetryNeighbor) const {
    if (pos <= m_cursor || pos > m_queue.size()) return false;

    for (int i = m_cursor; i < m_queue.size(); ++i) {
        if (m_queue[i].wordId != wordId) continue;
        const int distance = (i < pos) ? (pos - i) : (i + 1 - pos);
        if (distance <= minSameWordGap) return false;
    }

    if (avoidRetryNeighbor) {
        if (pos > m_cursor + 1 && !m_queue[pos - 1].counted) return false;
        if (pos < m_queue.size() && !m_queue[pos].counted) return false;
    }

    return true;
}

int ReviewSession::answeredCount() const {
    int n = 0;
    for (int i = 0; i < m_cursor && i < m_queue.size(); ++i) {
        if (m_queue[i].counted) ++n;
    }
    return n;
}

int ReviewSession::displayIndex() const {
    const int countedDone = answeredCount();
    if (currentIsCounted()) return countedDone + 1;
    return countedDone > 0 ? countedDone : 1;
}

void ReviewSession::applyResult(WordState& st, bool correct, qint64 now) {
    st.totalAttempts += 1;
    st.lastReviewAt = now;
    st.status = 2; // 复习过的词归入复习池

    if (correct) {
        st.totalCorrect += 1;
        st.streakCorrect += 1;
        // 连对 ≥3 → 移除高错误标记
        if (st.errorFlag && st.streakCorrect >= 3) st.errorFlag = 0;
        st.dueAt = now + stageIntervalSecs(st.ebbinghausStage);
        // 本次按当前阶段安排，下次答对再使用下一档间隔。
        if (st.ebbinghausStage < 5) st.ebbinghausStage += 1;
    } else {
        st.streakCorrect = 0;
        st.errorFlag = 1;
        // 回退阶段，尽快再复习（最短间隔）
        if (st.ebbinghausStage > 0) st.ebbinghausStage -= 1;
        st.dueAt = now + stageIntervalSecs(0);
    }
}

} // namespace word
