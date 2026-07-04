#include "DictDatabase.h"

#include <QDir>
#include <QFileInfo>
#include <QRandomGenerator>
#include <QSet>

#include <algorithm>

namespace word {

// 把 ECDICT 的字面 "\n" 替换为真正的换行，便于 QML 展示。
static QString normalizeText(const QString& text) {
    QString out = text;
    out.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    return out;
}

static QString wordKey(const QString& text) {
    const QString lower = text.toLower();
    QString best;
    QString current;
    for (const QChar ch : lower) {
        const ushort u = ch.unicode();
        if (u >= 'a' && u <= 'z') {
            current.append(ch);
        } else {
            if (current.size() > best.size()) best = current;
            current.clear();
        }
    }
    if (current.size() > best.size()) best = current;
    return best;
}

static QString stripKnownPrefix(const QString& key) {
    static const char* const kPrefixes[] = {
        "counter", "under", "inter", "trans", "super", "micro", "macro",
        "anti", "auto", "semi", "over", "post", "fore", "hyper", "sub",
        "pre", "pro", "mis", "dis", "non", "out", "uni", "bi", "tri",
        "re", "un", "de", "in", "im", "il", "ir", "en", "em", "co"
    };
    for (const char* prefix : kPrefixes) {
        const QString p = QLatin1String(prefix);
        if (key.startsWith(p) && key.size() - p.size() >= 4)
            return key.mid(p.size());
    }
    return key;
}

static QString stripKnownSuffix(const QString& key) {
    static const char* const kSuffixes[] = {
        "ification", "ization", "isation", "ational", "fulness", "ousness", "iveness",
        "lessness", "ability", "ibility", "ically", "ation", "ition",
        "ingly", "edly", "ments", "ness", "less", "able", "ible",
        "tion", "sion", "ment", "ence", "ance", "ship", "hood",
        "ical", "ally", "ious", "eous", "ous", "ive", "ful",
        "ity", "ism", "ist", "ize", "ise", "ify", "ing", "ied", "ies",
        "ed", "er", "est", "ly", "al", "ic", "s"
    };
    for (const char* suffix : kSuffixes) {
        const QString s = QLatin1String(suffix);
        if (!key.endsWith(s)) continue;
        if (s == QLatin1String("ies") && key.size() > 5)
            return key.left(key.size() - 3) + QLatin1Char('y');
        if (s == QLatin1String("ied") && key.size() > 5)
            return key.left(key.size() - 3) + QLatin1Char('y');
        if (key.size() - s.size() >= 4)
            return key.left(key.size() - s.size());
    }
    return key;
}

static QString roughRoot(const QString& key) {
    return stripKnownSuffix(stripKnownPrefix(key));
}

static QString consonantSkeleton(const QString& key) {
    QString out;
    QChar last;
    for (const QChar ch : key) {
        if (ch == QLatin1Char('a') || ch == QLatin1Char('e') ||
            ch == QLatin1Char('i') || ch == QLatin1Char('o') ||
            ch == QLatin1Char('u')) {
            continue;
        }
        QChar normalized = ch;
        if (normalized == QLatin1Char('k') || normalized == QLatin1Char('q'))
            normalized = QLatin1Char('c');
        if (normalized == last) continue;
        out.append(normalized);
        last = normalized;
    }
    return out;
}

static int commonPrefixLen(const QString& a, const QString& b) {
    const int n = qMin(a.size(), b.size());
    int i = 0;
    while (i < n && a.at(i) == b.at(i)) ++i;
    return i;
}

static int commonSuffixLen(const QString& a, const QString& b) {
    const int n = qMin(a.size(), b.size());
    int i = 0;
    while (i < n && a.at(a.size() - 1 - i) == b.at(b.size() - 1 - i)) ++i;
    return i;
}

static int sharedKnownAffixScore(const QString& a, const QString& b) {
    static const char* const kAffixes[] = {
        "counter", "under", "inter", "trans", "super", "micro", "macro",
        "anti", "auto", "semi", "over", "post", "fore", "hyper", "sub",
        "pre", "pro", "mis", "dis", "non", "out", "uni", "bi", "tri",
        "re", "un", "de", "in", "im", "il", "ir", "en", "em", "co",
        "ification", "ization", "isation", "ation", "ition", "tion", "sion", "ment",
        "ness", "less", "able", "ible", "ence", "ance", "ship", "hood",
        "ical", "ious", "eous", "ous", "ive", "ful", "ity", "ism",
        "ist", "ize", "ise", "ify", "ing", "ed", "er", "est", "ly", "al", "ic"
    };
    int score = 0;
    for (const char* affix : kAffixes) {
        const QString item = QLatin1String(affix);
        if ((a.startsWith(item) && b.startsWith(item)) ||
            (a.endsWith(item) && b.endsWith(item))) {
            score = qMax(score, 18 + item.size() * 2);
        }
    }
    return score;
}

static int morphologyScore(const QString& correct, const QString& candidate) {
    const QString a = wordKey(correct);
    const QString b = wordKey(candidate);
    if (a.size() < 3 || b.size() < 3 || a == b) return 0;

    int score = 0;
    const QString rootA = roughRoot(a);
    const QString rootB = roughRoot(b);
    if (rootA.size() >= 4 && rootA == rootB)
        score += 120 + rootA.size() * 4;

    const QString skeletonA = consonantSkeleton(rootA);
    const QString skeletonB = consonantSkeleton(rootB);
    if (skeletonA.size() >= 2 && skeletonA == skeletonB &&
        rootA.size() >= 3 && rootB.size() >= 3) {
        score += 90 + skeletonA.size() * 8;
    }

    const int prefix = commonPrefixLen(a, b);
    if (prefix >= 4) score += 28 + prefix * 4;

    const int suffix = commonSuffixLen(a, b);
    if (suffix >= 3) score += 22 + suffix * 4;

    score += sharedKnownAffixScore(a, b);

    if (rootA.size() >= 4 && rootB.size() >= 4) {
        const int rootPrefix = commonPrefixLen(rootA, rootB);
        const int rootSuffix = commonSuffixLen(rootA, rootB);
        if (rootPrefix >= 4) score += 35 + rootPrefix * 3;
        if (rootSuffix >= 3) score += 18 + rootSuffix * 3;
    }

    return score >= 40 ? score : 0;
}

// 解析 exchange 字段为「传统变形」可读文本，如 "过去式: did  过去分词: done"。
static QString formatExchange(const QString& exchange) {
    if (exchange.isEmpty()) return QString();

    // 类型码 -> 中文标签
    struct Item { QChar code; const char* label; };
    static const Item kItems[] = {
        {'p', "过去式"}, {'d', "过去分词"}, {'i', "现在分词"},
        {'3', "三单"},   {'r', "比较级"},   {'t', "最高级"},
        {'s', "复数"},
    };

    QStringList parts;
    const QStringList tokens = exchange.split('/', Qt::SkipEmptyParts);
    for (const QString& token : tokens) {
        const int sep = token.indexOf(':');
        if (sep <= 0) continue;
        const QChar code = token.at(0);
        const QString value = token.mid(sep + 1).trimmed();
        if (value.isEmpty()) continue;
        for (const auto& it : kItems) {
            if (it.code == code) {
                parts << QStringLiteral("%1: %2").arg(QString::fromUtf8(it.label), value);
                break;
            }
        }
    }
    return parts.join(QStringLiteral("   "));
}

QVariantMap WordEntry::toVariantMap() const {
    QVariantMap m;
    m[QStringLiteral("id")]          = id;
    m[QStringLiteral("word")]        = word;
    m[QStringLiteral("phonetic")]    = phonetic;
    m[QStringLiteral("definition")]  = normalizeText(definition);
    m[QStringLiteral("translation")] = normalizeText(translation);
    m[QStringLiteral("pos")]         = pos;
    m[QStringLiteral("tag")]         = tag;
    m[QStringLiteral("exchange")]    = formatExchange(exchange);
    return m;
}

// ---------------------------------------------------------------------------

int DictDatabase::scan(const QString& dictsDir) {
    m_dictsDir = dictsDir;
    m_dicts.clear();

    QDir dir(dictsDir);
    if (!dir.exists()) return 0;

    const QStringList files = dir.entryList(QStringList() << QStringLiteral("*.db"),
                                            QDir::Files, QDir::Name);
    for (const QString& file : files) {
        const QString fullPath = dir.absoluteFilePath(file);

        Sqlite probe;
        if (!probe.open(fullPath, /*readOnly*/ true)) continue;

        DictInfo info;
        info.fileName = file;
        info.dictId   = QFileInfo(file).completeBaseName();

        // 读取 dict_meta
        Stmt stmt = probe.prepare(QStringLiteral("SELECT key,value FROM dict_meta"));
        while (stmt.step()) {
            const QString key = stmt.columnText(0);
            const QString val = stmt.columnText(1);
            if (key == QStringLiteral("name"))             info.name = val;
            else if (key == QStringLiteral("description"))  info.description = val;
            else if (key == QStringLiteral("version"))      info.version = val;
            else if (key == QStringLiteral("word_count"))   info.wordCount = val.toInt();
        }
        if (info.name.isEmpty()) info.name = info.dictId;
        // word_count 缺失时实际统计一次
        if (info.wordCount <= 0) {
            Stmt cnt = probe.prepare(QStringLiteral("SELECT count(*) FROM words"));
            if (cnt.step()) info.wordCount = cnt.columnInt(0);
        }

        m_dicts.append(info);
    }
    return m_dicts.size();
}

bool DictDatabase::open(const QString& dictId) {
    for (const DictInfo& info : m_dicts) {
        if (info.dictId == dictId) {
            const QString fullPath = QDir(m_dictsDir).absoluteFilePath(info.fileName);
            if (m_db.open(fullPath, /*readOnly*/ true)) {
                m_current = info;
                return true;
            }
            return false;
        }
    }
    return false;
}

WordEntry DictDatabase::wordById(int id) {
    WordEntry e;
    if (!m_db.isOpen()) return e;
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT id,word,phonetic,definition,translation,pos,tag,frq,exchange "
        "FROM words WHERE id=?"));
    stmt.bind(1, id);
    if (stmt.step()) {
        e.id          = stmt.columnInt(0);
        e.word        = stmt.columnText(1);
        e.phonetic    = stmt.columnText(2);
        e.definition  = stmt.columnText(3);
        e.translation = stmt.columnText(4);
        e.pos         = stmt.columnText(5);
        e.tag         = stmt.columnText(6);
        e.frq         = stmt.columnInt(7);
        e.exchange    = stmt.columnText(8);
    }
    return e;
}

QVector<int> DictDatabase::allWordIdsByFreq() {
    QVector<int> ids;
    if (!m_db.isOpen()) return ids;
    ids.reserve(m_current.wordCount);
    // frq=0（无词频）排到最后，其余按 frq 升序（高频在前）
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT id FROM words "
        "ORDER BY CASE WHEN frq>0 THEN 0 ELSE 1 END, frq ASC, id ASC"));
    while (stmt.step()) ids.append(stmt.columnInt(0));
    return ids;
}

QStringList DictDatabase::randomTranslations(int count, int excludeId) {
    QStringList out;
    if (!m_db.isOpen() || count <= 0) return out;
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT translation FROM words WHERE id<>? AND translation<>'' "
        "ORDER BY RANDOM() LIMIT ?"));
    stmt.bind(1, excludeId);
    stmt.bind(2, count);
    while (stmt.step()) {
        out << stmt.columnText(0).replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    }
    return out;
}

QStringList DictDatabase::difficultTranslations(int count, int excludeId,
                                                const QString& correctWord,
                                                const QString& correctTranslation) {
    QStringList out;
    if (!m_db.isOpen() || count <= 0) return out;

    const QString correctWordKey = wordKey(correctWord);
    const QString correctTranslationKey = correctTranslation.trimmed();
    if (correctWordKey.size() < 3)
        return randomTranslations(count, excludeId);

    struct Candidate {
        QString translation;
        QString key;
        int score = 0;
        int tie = 0;
    };

    QVector<Candidate> candidates;
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT word,translation FROM words "
        "WHERE id<>? AND word<>'' AND translation<>''"));
    stmt.bind(1, excludeId);
    while (stmt.step()) {
        const QString word = stmt.columnText(0).trimmed();
        const QString translation = normalizeText(stmt.columnText(1)).trimmed();
        const QString key = translation.trimmed();
        if (key.isEmpty() || key == correctTranslationKey) continue;
        const int score = morphologyScore(correctWord, word);
        if (score <= 0) continue;

        Candidate candidate;
        candidate.translation = translation;
        candidate.key = key;
        candidate.score = score;
        candidate.tie = int(QRandomGenerator::global()->bounded(1000000));
        candidates.append(candidate);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  if (a.score != b.score) return a.score > b.score;
                  return a.tie < b.tie;
              });

    QSet<QString> seen;
    seen.insert(correctTranslationKey);
    for (const Candidate& candidate : candidates) {
        if (out.size() >= count) break;
        if (seen.contains(candidate.key)) continue;
        seen.insert(candidate.key);
        out << candidate.translation;
    }

    if (out.size() < count) {
        const QStringList fallback = randomTranslations(count * 4 + 8, excludeId);
        for (const QString& translation : fallback) {
            if (out.size() >= count) break;
            const QString key = translation.trimmed();
            if (key.isEmpty() || seen.contains(key)) continue;
            seen.insert(key);
            out << translation;
        }
    }

    return out;
}

QStringList DictDatabase::randomWords(int count, int excludeId) {
    QStringList out;
    if (!m_db.isOpen() || count <= 0) return out;
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT word FROM words WHERE id<>? ORDER BY RANDOM() LIMIT ?"));
    stmt.bind(1, excludeId);
    stmt.bind(2, count);
    while (stmt.step()) out << stmt.columnText(0);
    return out;
}

QStringList DictDatabase::difficultWords(int count, int excludeId, const QString& correctWord) {
    QStringList out;
    if (!m_db.isOpen() || count <= 0) return out;

    const QString correctKey = wordKey(correctWord);
    if (correctKey.size() < 3)
        return randomWords(count, excludeId);

    struct Candidate {
        QString word;
        QString key;
        int score = 0;
        int tie = 0;
    };

    QVector<Candidate> candidates;
    Stmt stmt = m_db.prepare(QStringLiteral(
        "SELECT word FROM words WHERE id<>? AND word<>''"));
    stmt.bind(1, excludeId);
    while (stmt.step()) {
        const QString word = stmt.columnText(0).trimmed();
        const QString key = wordKey(word);
        if (key.isEmpty() || key == correctKey) continue;
        const int score = morphologyScore(correctWord, word);
        if (score <= 0) continue;
        Candidate candidate;
        candidate.word = word;
        candidate.key = key;
        candidate.score = score;
        candidate.tie = int(QRandomGenerator::global()->bounded(1000000));
        candidates.append(candidate);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) {
                  if (a.score != b.score) return a.score > b.score;
                  return a.tie < b.tie;
              });

    QSet<QString> seen;
    seen.insert(correctKey);
    for (const Candidate& candidate : candidates) {
        if (out.size() >= count) break;
        if (seen.contains(candidate.key)) continue;
        seen.insert(candidate.key);
        out << candidate.word;
    }

    if (out.size() < count) {
        const QStringList fallback = randomWords(count * 4 + 8, excludeId);
        for (const QString& word : fallback) {
            if (out.size() >= count) break;
            const QString key = wordKey(word);
            if (key.isEmpty() || seen.contains(key)) continue;
            seen.insert(key);
            out << word;
        }
    }

    return out;
}

} // namespace word
