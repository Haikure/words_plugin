#pragma once

// ---------------------------------------------------------------------------
// DictDatabase —— 词库（只读）访问层
//
// 职责：
//   * 扫描 dicts/ 目录下所有 .db 词库，读取其 dict_meta 元信息
//   * 打开/切换当前词库
//   * 按 id 查单词、按词频列出待学单词、随机抽取复习干扰项
// 词库为只读，随插件分发；一个 .db 文件对应一个词库。
// ---------------------------------------------------------------------------

#include "Sqlite.h"

#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace word {

// 单词条目（与 words 表字段对应）
struct WordEntry {
    int     id = 0;
    QString word;
    QString phonetic;
    QString definition;
    QString translation;
    QString pos;
    QString tag;
    int     frq = 0;
    QString exchange;

    bool isValid() const { return id > 0; }
    // 转成给 QML 用的 map（已处理换行、解析 exchange）
    QVariantMap toVariantMap() const;
};

// 一个词库的元信息（来自 dict_meta + 文件名）
struct DictInfo {
    QString dictId;     // 词库唯一标识（取文件名去扩展名）
    QString fileName;   // 文件名（含 .db）
    QString name;       // 显示名称
    QString description;
    QString version;
    int     wordCount = 0;
};

class DictDatabase {
public:
    DictDatabase() = default;

    // 扫描词库目录，填充可用词库列表。返回找到的词库数。
    int scan(const QString& dictsDir);

    const QVector<DictInfo>& dicts() const { return m_dicts; }

    // 打开指定 dictId 的词库为当前词库。
    bool open(const QString& dictId);
    bool isOpen() const { return m_db.isOpen(); }

    const DictInfo& currentInfo() const { return m_current; }
    QString currentDictId() const { return m_current.dictId; }
    int totalWords() const { return m_current.wordCount; }

    // 按词条 id 查询单词。
    WordEntry wordById(int id);

    // 返回所有词条 id，按当代词频 frq 升序（0 排最后），用于决定学习顺序。
    QVector<int> allWordIdsByFreq();

    // 随机抽取 count 个中文释义（排除 excludeId），作为复习干扰项。
    QStringList randomTranslations(int count, int excludeId);
    // 随机抽取 count 个单词（排除 excludeId），作为复习干扰项。
    QStringList randomWords(int count, int excludeId);

private:
    QString          m_dictsDir;
    QVector<DictInfo> m_dicts;
    DictInfo         m_current;
    Sqlite           m_db;
};

} // namespace word
