#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
check_dict.py —— 校验词库 SQLite 数据库是否符合背单词插件要求。

用法：
    python3 check_dict.py                 # 检查 ./dicts/ 下所有 .db
    python3 check_dict.py a.db b.db ...    # 检查指定文件

分两级报告：
    [错误] 会导致插件无法正常运行，必须修复
    [警告] 插件仍可运行，但存在隐患或体验下降

退出码：0=全部通过（无错误）；1=存在错误；2=用法/文件问题。
完整结构定义见 docs/词库格式说明.md。
"""

import glob
import os
import sqlite3
import sys

# words 表中插件 SQL 直接 SELECT 的列，缺失会导致查询失败 → 错误级
REQUIRED_WORD_COLUMNS = [
    "id", "word", "phonetic", "definition", "translation",
    "pos", "tag", "frq", "exchange",
]
# 文档定义、但插件未直接查询的列，缺失仅 → 警告级
OPTIONAL_WORD_COLUMNS = ["collins", "oxford", "bnc"]

EXCHANGE_CODES = set("pdi3rts01")  # exchange 合法类型码


class Report:
    """收集单个词库的错误与警告。"""

    def __init__(self, path):
        self.path = path
        self.errors = []
        self.warnings = []

    def error(self, msg, fix=None):
        self.errors.append((msg, fix))

    def warn(self, msg, fix=None):
        self.warnings.append((msg, fix))

    def dump(self):
        name = os.path.basename(self.path)
        if not self.errors and not self.warnings:
            print("✓ %s —— 通过，完全符合要求" % name)
            return
        status = "✗" if self.errors else "△"
        print("%s %s" % (status, name))
        for msg, fix in self.errors:
            print("    [错误] %s" % msg)
            if fix:
                print("           → 修复：%s" % fix)
        for msg, fix in self.warnings:
            print("    [警告] %s" % msg)
            if fix:
                print("           → 建议：%s" % fix)


def table_exists(conn, table):
    row = conn.execute(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?", (table,)
    ).fetchone()
    return row is not None


def column_names(conn, table):
    return [r[1] for r in conn.execute("PRAGMA table_info(%s)" % table).fetchall()]


def index_exists_on(conn, table, column):
    """检查 table.column 上是否存在索引。"""
    for idx in conn.execute("PRAGMA index_list(%s)" % table).fetchall():
        idx_name = idx[1]
        cols = [c[2] for c in conn.execute("PRAGMA index_info(%s)" % idx_name).fetchall()]
        if column in cols:
            return True
    return False


def check_dict_meta(conn, rep):
    """校验 dict_meta 表与词库名称。"""
    if not table_exists(conn, "dict_meta"):
        rep.warn("缺少 dict_meta 表（词库名称将回退为文件名）",
                 "建表：CREATE TABLE dict_meta(key TEXT PRIMARY KEY, value TEXT)，"
                 "并写入 name 键")
        return
    cols = column_names(conn, "dict_meta")
    if "key" not in cols or "value" not in cols:
        rep.error("dict_meta 表结构错误，缺少 key/value 列",
                  "应为 dict_meta(key TEXT PRIMARY KEY, value TEXT)")
        return
    row = conn.execute("SELECT value FROM dict_meta WHERE key='name'").fetchone()
    if not row or not (row[0] or "").strip():
        rep.warn("dict_meta 未设置 name（词库显示名）",
                 "INSERT INTO dict_meta(key,value) VALUES('name','你的词库名')")


def check_words_schema(conn, rep):
    """校验 words 表是否存在及列是否齐全。返回列集合（不存在则 None）。"""
    if not table_exists(conn, "words"):
        rep.error("缺少 words 表（核心数据表）",
                  "参照 docs/词库格式说明.md 建立 words 表")
        return None
    cols = set(column_names(conn, "words"))
    for c in REQUIRED_WORD_COLUMNS:
        if c not in cols:
            rep.error("words 表缺少必需列 `%s`（插件查询会失败）" % c,
                      "为 words 表补齐该列；列定义见 docs/词库格式说明.md")
    for c in OPTIONAL_WORD_COLUMNS:
        if c not in cols:
            rep.warn("words 表缺少可选列 `%s`" % c,
                     "补齐以保持结构完整（值可填 0）")
    return cols


def check_words_data(conn, rep, cols):
    """校验 words 表数据质量。"""
    total = conn.execute("SELECT count(*) FROM words").fetchone()[0]
    if total == 0:
        rep.error("words 表为空（没有任何单词）", "导入单词数据")
        return
    print("    词条总数：%d" % total)

    # word 为空 → 错误
    if "word" in cols:
        n = conn.execute(
            "SELECT count(*) FROM words WHERE word IS NULL OR trim(word)=''"
        ).fetchone()[0]
        if n:
            rep.error("有 %d 条记录的 word 为空" % n,
                      "删除这些行或补上单词")
        # word 重复 → 警告
        dup = conn.execute(
            "SELECT count(*) FROM (SELECT word FROM words "
            "GROUP BY word HAVING count(*)>1)"
        ).fetchone()[0]
        if dup:
            rep.warn("有 %d 个单词重复出现" % dup, "去重以避免学习时重复出现")

    # translation 为空 → 警告（看中文选英文题干会空）
    if "translation" in cols:
        n = conn.execute(
            "SELECT count(*) FROM words WHERE translation IS NULL OR trim(translation)=''"
        ).fetchone()[0]
        if n:
            rep.warn("有 %d 条记录缺少 translation（中文释义）" % n,
                     "补全中文释义，否则这些词作为复习选项/题干时会空白")

    # frq 全为 0 → 警告（学习顺序退化）
    if "frq" in cols:
        nonzero = conn.execute("SELECT count(*) FROM words WHERE frq>0").fetchone()[0]
        if nonzero == 0:
            rep.warn("所有词的 frq（词频）均为 0",
                     "填入词频可让插件优先学习高频词；否则按录入顺序学习")

    # id 重复（理论上主键不会，但兼容非主键情形）
    if "id" in cols:
        dup_id = conn.execute(
            "SELECT count(*) FROM (SELECT id FROM words GROUP BY id HAVING count(*)>1)"
        ).fetchone()[0]
        if dup_id:
            rep.error("有 %d 个重复的 id" % dup_id, "确保 id 唯一（建议设为 INTEGER PRIMARY KEY）")

    # 数值列抽样校验是否为整数
    for c in ["collins", "oxford", "bnc", "frq"]:
        if c in cols:
            bad = conn.execute(
                "SELECT count(*) FROM words WHERE %s IS NOT NULL "
                "AND typeof(%s) NOT IN ('integer','null')" % (c, c)
            ).fetchone()[0]
            if bad:
                rep.warn("列 `%s` 有 %d 条非整数值" % (c, bad),
                         "数值列请存整数，空缺用 0")

    # exchange 格式抽样校验
    if "exchange" in cols:
        rows = conn.execute(
            "SELECT word, exchange FROM words "
            "WHERE exchange IS NOT NULL AND exchange<>'' LIMIT 500"
        ).fetchall()
        bad_samples = []
        for word, ex in rows:
            for token in ex.split("/"):
                if not token:
                    continue
                if ":" not in token or token[0] not in EXCHANGE_CODES:
                    bad_samples.append((word, token))
                    break
        if bad_samples:
            example = "，".join("%s→%s" % (w, t) for w, t in bad_samples[:3])
            rep.warn("exchange 字段格式可疑（抽样 %d 例，如 %s）"
                     % (len(bad_samples), example),
                     "格式应为 `类型码:变形/...`，类型码取 p d i 3 r t s 0 1，"
                     "见 docs/词库格式说明.md")


def check_index(conn, rep, cols):
    if cols and "word" in cols and not index_exists_on(conn, "words", "word"):
        rep.warn("words.word 上没有索引（大词库查询会变慢）",
                 "CREATE INDEX idx_words_word ON words(word)")


def check_word_count_consistency(conn, rep):
    """校验 dict_meta.word_count 与实际行数是否一致。"""
    if not table_exists(conn, "dict_meta") or not table_exists(conn, "words"):
        return
    row = conn.execute("SELECT value FROM dict_meta WHERE key='word_count'").fetchone()
    if not row:
        return
    try:
        declared = int(row[0])
    except (ValueError, TypeError):
        rep.warn("dict_meta.word_count 不是整数", "填入实际词条数")
        return
    actual = conn.execute("SELECT count(*) FROM words").fetchone()[0]
    if declared != actual:
        rep.warn("dict_meta.word_count(%d) 与实际行数(%d)不一致" % (declared, actual),
                 "更新为实际行数 %d" % actual)


def check_one(path):
    rep = Report(path)
    if not os.path.isfile(path):
        rep.error("文件不存在", None)
        rep.dump()
        return False

    # 是否为合法 SQLite 文件
    try:
        with open(path, "rb") as f:
            if f.read(16) != b"SQLite format 3\x00":
                rep.error("不是合法的 SQLite 数据库文件", "确认文件未损坏、确为 .db")
                rep.dump()
                return False
    except OSError as e:
        rep.error("无法读取文件：%s" % e, None)
        rep.dump()
        return False

    try:
        conn = sqlite3.connect(path)
    except sqlite3.Error as e:
        rep.error("无法打开数据库：%s" % e, None)
        rep.dump()
        return False

    try:
        check_dict_meta(conn, rep)
        cols = check_words_schema(conn, rep)
        if cols is not None:
            check_words_data(conn, rep, cols)
            check_index(conn, rep, cols)
        check_word_count_consistency(conn, rep)
    except sqlite3.Error as e:
        rep.error("校验过程中数据库出错：%s" % e, None)
    finally:
        conn.close()

    rep.dump()
    return len(rep.errors) == 0


def main():
    args = sys.argv[1:]
    if args:
        targets = args
    else:
        targets = sorted(glob.glob(os.path.join("dicts", "*.db")))
        if not targets:
            print("未指定文件，且 ./dicts/ 下没有 .db。", file=sys.stderr)
            print("用法：python3 check_dict.py [词库.db ...]", file=sys.stderr)
            return 2

    print("检查 %d 个词库：\n" % len(targets))
    all_ok = True
    for path in targets:
        if not check_one(path):
            all_ok = False
        print()

    if all_ok:
        print("结论：全部通过 ✓")
        return 0
    print("结论：存在不符合要求的词库，请按上面的[错误]修复。")
    return 1


if __name__ == "__main__":
    sys.exit(main())
