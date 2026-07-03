# Wiktionary 词组词库生成说明

本文说明 `tools/make_wiktionary_phrases.py` 的用途和用法。
该工具会从 English Wiktionary 抓取英语词组条目，并生成本插件可直接识别的 SQLite 词库 `.db`。

生成出的数据库结构与 `docs/dict_introduction.md` 完全一致，可以直接放在 `dicts/` 目录下使用。

---

## 一、数据来源与筛选逻辑

默认抓取两个 Wiktionary 分类：

- `Category:English phrasal verbs`
- `Category:English prepositional phrases`

Wiktionary 没有“高中英语”官方标签，因此脚本使用本仓库已有词库做级别过滤：

- 默认读取 `dicts/gaokao.db`
- 只有词组中大部分组成词出现在该词库里，才会进入候选
- 只保留 Wiktionary 条目中带 Mandarin/Chinese 翻译的词组
- 自动抽取英文释义、中文释义和词性
- 默认标记 `tag` 为 `gk`
- `frq` 按筛选后的推荐顺序写入，用于插件学习顺序

脚本还会过滤一批明显不适合高中学习场景的成人、粗俗、暴力和毒品相关义项，并对常见繁体中文翻译做轻量简化。

> 注意：这不是人工校订词表。Wiktionary 是协作词典，释义质量和中文翻译覆盖率取决于页面本身。正式发版前建议抽样检查。

---

## 二、基本用法

在仓库根目录执行：

```bash
python3 tools/make_wiktionary_phrases.py dicts/gk_wiktionary_phrases.db \
  --name "高中英语词组-维基词典" \
  --description "English Wiktionary phrasal verbs and prepositional phrases filtered by gaokao vocabulary with Mandarin translations"
```

生成后用检查工具确认数据库结构：

```bash
python3 tools/check_dict.py dicts/gk_wiktionary_phrases.db
```

检查通过后，插件启动时会自动扫描 `dicts/` 下的 `.db` 文件，并在词库选择页显示 `dict_meta.name`。

---

## 三、常用参数

| 参数 | 默认值 | 说明 |
| ---- | ------ | ---- |
| `output_db` | 必填 | 输出 SQLite 词库路径 |
| `--level-db` | `dicts/gaokao.db` | 用作级别过滤的本地词库 |
| `--category` | 默认两个分类 | Wiktionary 分类，可重复指定 |
| `--target` | `500` | 达到该保留条数后停止；`0` 表示不限制 |
| `--max-pages` | `1800` | 级别过滤后最多请求多少个候选页面；`0` 表示不限制 |
| `--max-members-per-category` | `0` | 每个分类最多扫描多少成员；`0` 表示不限制 |
| `--min-words` | `2` | 词组最少单词数 |
| `--max-words` | `5` | 词组最多单词数 |
| `--min-known-ratio` | `0.8` | 组成词出现在 `--level-db` 中的最低比例 |
| `--tag` | `gk` | 写入 `words.tag` 的考试标签 |
| `--name` | `Wiktionary gaokao phrases` | 写入 `dict_meta.name` |
| `--description` | 内置英文说明 | 写入 `dict_meta.description` |
| `--version` | `1.0.0` | 写入 `dict_meta.version` |

---

## 四、示例

### 1. 生成默认高中词组库

```bash
python3 tools/make_wiktionary_phrases.py dicts/gk_wiktionary_phrases.db \
  --name "高中英语词组-维基词典"
```

### 2. 只抓介词短语

```bash
python3 tools/make_wiktionary_phrases.py dicts/gk_wiktionary_prep_phrases.db \
  --category "Category:English prepositional phrases" \
  --name "高中英语介词短语-维基词典"
```

### 3. 放宽词汇级别过滤

如果想多抓一些词组，可以降低 `--min-known-ratio`：

```bash
python3 tools/make_wiktionary_phrases.py dicts/gk_wiktionary_phrases.db \
  --min-known-ratio 0.6 \
  --max-pages 3000 \
  --name "高中英语词组-维基词典"
```

### 4. 使用其它本地词库作为过滤基础

```bash
python3 tools/make_wiktionary_phrases.py dicts/cet4_wiktionary_phrases.db \
  --level-db dicts/cet4.db \
  --tag cet4 \
  --name "四级英语词组-维基词典"
```

---

## 五、字段写入规则

输出数据库中的 `words` 字段大致这样填写：

| 字段 | 写入内容 |
| ---- | -------- |
| `word` | Wiktionary 英语词组标题，统一小写并压缩空格 |
| `phonetic` | 留空 |
| `definition` | Wiktionary 英文释义，多个释义用字面 `\n` 分隔 |
| `translation` | Wiktionary Mandarin/Chinese 翻译，多个译文用字面 `\n` 分隔 |
| `pos` | 从 Wiktionary 词性标题推断，如 `v.phr.`、`prep.phr.` |
| `collins` | `0` |
| `oxford` | `0` |
| `tag` | 参数 `--tag` |
| `bnc` | `0` |
| `frq` | 生成顺序，从 `1` 开始 |
| `exchange` | 留空 |

`dict_meta` 会写入 `name`、`description`、`version`、`word_count`、`created_at`。

---

## 六、质量检查建议

生成后建议至少执行：

```bash
python3 tools/check_dict.py dicts/gk_wiktionary_phrases.db
sqlite3 -header -column dicts/gk_wiktionary_phrases.db \
  "select id, word, pos, translation, definition from words order by frq limit 30;"
```

如果要重点检查介词短语：

```bash
sqlite3 -header -column dicts/gk_wiktionary_phrases.db \
  "select id, word, pos, translation from words where pos like '%prep%' order by frq limit 50;"
```

如果发现不适合的词组或释义，优先在 `tools/make_wiktionary_phrases.py` 的过滤规则中修正，再重新生成数据库。这样结果可复现，也方便之后更新词库。
