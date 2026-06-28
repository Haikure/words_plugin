#include "StudySession.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QtGlobal>

namespace word {

static const int kMaxStudyQueueLen = 256;

void StudySession::start(const QVector<int>& newWords) {
    m_queue.clear();
    m_cursor = 0;
    m_roundNewWords = newWords;
    m_totalNew = newWords.size();
    m_pendingReinforce = 0;

    for (int i = 0; i < newWords.size(); ++i)
        m_queue.append({newWords[i], true, 0});
}

QString StudySession::toJson() const {
    QJsonArray queue;
    for (const Item& it : m_queue) {
        QJsonObject o;
        o[QStringLiteral("w")] = it.wordId;
        o[QStringLiteral("c")] = it.counted;
        o[QStringLiteral("r")] = it.reinforceLevel;
        queue.append(o);
    }
    QJsonArray newWords;
    for (int id : m_roundNewWords) newWords.append(id);

    QJsonObject root;
    root[QStringLiteral("queue")]    = queue;
    root[QStringLiteral("cursor")]   = m_cursor;
    root[QStringLiteral("totalNew")] = m_totalNew;
    root[QStringLiteral("newWords")] = newWords;
    root[QStringLiteral("pending")]  = m_pendingReinforce;
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

bool StudySession::fromJson(const QString& json) {
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) return false;
    const QJsonObject root = doc.object();

    m_queue.clear();
    const QJsonArray queue = root[QStringLiteral("queue")].toArray();
    for (const QJsonValue& v : queue) {
        const QJsonObject o = v.toObject();
        m_queue.append({
            o[QStringLiteral("w")].toInt(),
            o[QStringLiteral("c")].toBool(),
            o[QStringLiteral("r")].toInt()
        });
    }
    m_cursor   = root[QStringLiteral("cursor")].toInt();
    m_totalNew = root[QStringLiteral("totalNew")].toInt();
    m_pendingReinforce = root[QStringLiteral("pending")].toInt();

    m_roundNewWords.clear();
    const QJsonArray newWords = root[QStringLiteral("newWords")].toArray();
    for (const QJsonValue& v : newWords) m_roundNewWords.append(v.toInt());

    if (m_queue.isEmpty()) return false;
    if (m_cursor < 0 || m_cursor > m_queue.size()) return false;
    if (m_totalNew < 0) return false;
    if (m_pendingReinforce < 0) m_pendingReinforce = 0;
    return true;
}

int StudySession::currentWordId() const {
    if (m_cursor < 0 || m_cursor >= m_queue.size()) return -1;
    return m_queue[m_cursor].wordId;
}

bool StudySession::currentIsReview() const {
    if (m_cursor < 0 || m_cursor >= m_queue.size()) return false;
    return !m_queue[m_cursor].counted;
}

bool StudySession::currentCounted() const {
    if (m_cursor < 0 || m_cursor >= m_queue.size()) return false;
    return m_queue[m_cursor].counted;
}

bool StudySession::currentNeedsReview() const {
    if (m_cursor < 0 || m_cursor >= m_queue.size()) return false;
    return m_queue[m_cursor].reinforceLevel > 0;
}

int StudySession::currentReinforceLevel() const {
    if (m_cursor < 0 || m_cursor >= m_queue.size()) return 0;
    return m_queue[m_cursor].reinforceLevel;
}

void StudySession::submitRating(StudyRating rating) {
    if (m_cursor < 0 || m_cursor >= m_queue.size()) return;

    const Item current = m_queue[m_cursor];
    if (current.reinforceLevel > 0 && m_pendingReinforce > 0)
        --m_pendingReinforce;

    if (rating == StudyRating::Unknown) {
        insertDelayedReinforce(current.wordId, 3, 6, 3, 2);
        insertDelayedReinforce(current.wordId, 6, 12, 4, 1);
    }

    ++m_cursor;
}

void StudySession::insertDelayedReinforce(int wordId, int minGap, int maxGap, int minSameWordGap, int level) {
    if (m_queue.size() >= kMaxStudyQueueLen) return;

    const int afterCurrent = m_cursor + 1;
    if (afterCurrent >= m_queue.size()) return;

    if (maxGap < minGap) maxGap = minGap;
    const int lower = qMin(afterCurrent + minGap, m_queue.size());
    const int upper = qMin(afterCurrent + maxGap, m_queue.size());

    QVector<int> positions;
    for (int pos = lower; pos <= upper; ++pos) {
        if (canInsertReinforceAt(wordId, pos, minSameWordGap, true))
            positions.append(pos);
    }

    if (positions.isEmpty()) {
        for (int pos = lower; pos <= upper; ++pos) {
            if (canInsertReinforceAt(wordId, pos, minSameWordGap, false))
                positions.append(pos);
        }
    }

    int pos = -1;
    if (!positions.isEmpty()) {
        pos = positions[QRandomGenerator::global()->bounded(positions.size())];
    } else if (m_queue.size() > afterCurrent &&
               canInsertReinforceAt(wordId, m_queue.size(), minSameWordGap, false)) {
        pos = m_queue.size();
    }

    if (pos < 0) return;
    m_queue.insert(pos, {wordId, false, level});
    ++m_pendingReinforce;
}

bool StudySession::canInsertReinforceAt(int wordId, int pos, int minSameWordGap, bool avoidReinforceNeighbor) const {
    if (pos <= m_cursor || pos > m_queue.size()) return false;

    for (int i = m_cursor; i < m_queue.size(); ++i) {
        if (m_queue[i].wordId != wordId) continue;
        const int distance = (i < pos) ? (pos - i) : (i + 1 - pos);
        if (distance <= minSameWordGap) return false;
    }

    if (avoidReinforceNeighbor) {
        if (pos > m_cursor + 1 && !m_queue[pos - 1].counted) return false;
        if (pos < m_queue.size() && !m_queue[pos].counted) return false;
    }

    return true;
}

int StudySession::learnedIndex() const {
    // 统计 [0, cursor] 区间内的计数项数量
    int n = 0;
    for (int i = 0; i <= m_cursor && i < m_queue.size(); ++i) {
        if (m_queue[i].counted) ++n;
    }
    return n;
}

} // namespace word
