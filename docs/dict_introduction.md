# 背单词插件 · 词库数据库格式说明

本文档定义背单词插件使用的**词库数据库**（只读）的 SQLite 结构。
任何符合本结构的 `.db` 文件，放进插件的 `dicts/` 目录即可被识别为一个词库。

> 用途提示：你可以把本文档**整篇**交给 AI，并附上你已有词表的样例，
> 让它直接生成「你的词表 → 本插件词库 .db」的转换脚本。

---

## 一、总览

- 引擎：**SQLite 3**
- 编码：**UTF-8**
- 一个 `.db` 文件 = 一个独立词库
- 文件名建议用英文/数字（如 `cet6.db`），词库的中文显示名写在库内的 `dict_meta` 表中
- 共两张表：`dict_meta`（词库元信息）、`words`（单词表）

---

## 二、表结构

### 1. `dict_meta` —— 词库元信息（键值表）

```sql
CREATE TABLE dict_meta (
    key   TEXT PRIMARY KEY,
    value TEXT
);
```

约定的键（`name` 必填，其余可选）：

| key          | 说明                         | 示例                       |
| ------------ | ---------------------------- | -------------------------- |
| `name`       | **词库显示名称**（界面展示） | `高考+四级核心词`          |
| `description`| 词库描述                     | `ECDICT 高考与四级合并词库`|
| `version`    | 词库版本号                   | `1.0.0`                    |
| `word_count` | 词条总数（字符串形式）       | `5331`                     |
| `created_at` | 生成时间（Unix 秒，字符串）  | `1781846281`               |

### 2. `words` —— 单词表

```sql
CREATE TABLE words (
    id          INTEGER PRIMARY KEY,   -- 自增主键
    word        TEXT NOT NULL,         -- 单词本身
    phonetic    TEXT,                  -- 音标/注音（不含括号，如 əˈbændən）
    definition  TEXT,                  -- 英文释义
    translation TEXT,                  -- 中文释义
    pos         TEXT,                  -- 词性分布
    collins     INTEGER,               -- 柯林斯星级 0-5
    oxford      INTEGER,               -- 是否牛津3000核心词 0/1
    tag         TEXT,                  -- 考试标签，空格分隔
    bnc         INTEGER,               -- BNC 传统语料库词频排名
    frq         INTEGER,               -- 当代语料库词频排名
    exchange    TEXT                   -- 词形变化（见下）
);

CREATE INDEX idx_words_word ON words(word);
```

字段说明：

| 字段          | 必填 | 插件中的用途                                             |
| ------------- | ---- | -------------------------------------------------------- |
| `word`        | 是   | 学习/复习展示的英文单词                                  |
| `phonetic`    | 否   | 单词卡注音；看中文选英文时题目也会显示                   |
| `definition`  | 否   | 英文释义（可留空）                                       |
| `translation` | 是   | 中文释义；复习四选一的选项来源                           |
| `pos`         | 否   | 词性展示                                                 |
| `collins`     | 否   | 预留（暂不展示）                                         |
| `oxford`      | 否   | 预留                                                     |
| `tag`         | 否   | 单词卡顶部「单词类型」标签展示（如 `gk cet4`）           |
| `bnc`         | 否   | 预留                                                     |
| `frq`         | 否   | **决定学习顺序**：按 `frq` 升序优先学高频词（0 排最后）  |
| `exchange`    | 否   | 单词卡「传统变形」展示（过去式/复数等）                  |

> 数值字段（collins/oxford/bnc/frq）若无数据，请填 `0`，不要留 NULL 字符串。

---

## 三、`exchange` 词形变化格式

格式为 `类型:变形/类型:变形/...`，用 `/` 分隔不同项，`:` 前为类型码：

| 类型码 | 含义              |
| ------ | ----------------- |
| `p`    | 过去式            |
| `d`    | 过去分词          |
| `i`    | 现在分词          |
| `3`    | 第三人称单数      |
| `r`    | 形容词比较级      |
| `t`    | 形容词最高级      |
| `s`    | 名词复数          |
| `0`    | 原型（Lemma）     |
| `1`    | Lemma 的变化形式  |

示例：`perceive` 的 exchange 为
`d:perceived/p:perceived/3:perceives/i:perceiving`
表示过去式/过去分词为 perceived，三单为 perceives，现在分词为 perceiving。

插件会解析此字段，在单词卡上以「过去式：… 过去分词：…」的形式展示。
没有可留空字符串。

---

## 四、中文释义换行约定

ECDICT 的 `translation`/`definition` 中，多条释义之间用**字面的两个字符 `\n`**
（反斜杠 + n，并非真正的换行符）分隔。插件展示时会自动把 `\n` 替换为换行。
你的词库沿用此约定即可；若直接用真换行符也能正常显示。

---

## 五、最小可运行示例（Python）

```python
import sqlite3, time

conn = sqlite3.connect("my_dict.db")
conn.executescript("""
CREATE TABLE dict_meta (key TEXT PRIMARY KEY, value TEXT);
CREATE TABLE words (
    id INTEGER PRIMARY KEY, word TEXT NOT NULL, phonetic TEXT,
    definition TEXT, translation TEXT, pos TEXT,
    collins INTEGER, oxford INTEGER, tag TEXT,
    bnc INTEGER, frq INTEGER, exchange TEXT);
CREATE INDEX idx_words_word ON words(word);
""")

rows = [
    # word, phonetic, definition, translation, pos, collins, oxford, tag, bnc, frq, exchange
    ("abandon", "əˈbændən", "vt. to give up", "vt. 放弃, 抛弃", "vt.",
     3, 1, "cet4 gk", 3000, 2500, "d:abandoned/p:abandoned/i:abandoning/3:abandons"),
]
conn.executemany(
    "INSERT INTO words(word,phonetic,definition,translation,pos,"
    "collins,oxford,tag,bnc,frq,exchange) VALUES(?,?,?,?,?,?,?,?,?,?,?)", rows)

meta = {"name": "我的词库", "description": "示例", "version": "1.0.0",
        "word_count": str(len(rows)), "created_at": str(int(time.time()))}
conn.executemany("INSERT OR REPLACE INTO dict_meta(key,value) VALUES(?,?)",
                 list(meta.items()))
conn.commit(); conn.close()
```

---

## 六、用本仓库脚本转换 ECDICT 词表

`tools/make_dict.py` 可直接把 ECDICT 格式 CSV 转为本词库：

```bash
# 整表转换
python3 tools/make_dict.py input.csv output.db --name "词库名"

# 仅保留指定考试标签（如高考+四级）
python3 tools/make_dict.py ecdict.csv gk_cet4.db --name "高考四级" --tag-filter gk cet4
```

常见考试标签：`zk`(中考) `gk`(高考) `cet4`(四级) `cet6`(六级)
`ky`(考研) `toefl` `ielts` `gre`。
