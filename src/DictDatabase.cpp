#include "DictDatabase.h"

#include <QDir>
#include <QFileInfo>

namespace word {

// 把 ECDICT 的字面 "\n" 替换为真正的换行，便于 QML 展示。
static QString normalizeText(const QString& text) {
    QString out = text;
    out.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    return out;
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

} // namespace word
