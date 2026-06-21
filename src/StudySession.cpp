#include "StudySession.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

    const int originalRemaining = m_queue.size() - m_cursor - 1;
    int inserted = 0;
    auto insertReinforce = [&](int minGap, int level) {
        if (m_queue.size() >= kMaxStudyQueueLen) return;
        if (originalRemaining < minGap) return;
        const int pos = qMin(m_cursor + 1 + minGap + inserted, m_queue.size());
        m_queue.insert(pos, {current.wordId, false, level});
        ++m_pendingReinforce;
        ++inserted;
    };

    if (rating == StudyRating::Unknown) {
        insertReinforce(2, 2);
        insertReinforce(5, 1);
    }

    ++m_cursor;
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
