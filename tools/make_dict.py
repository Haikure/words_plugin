#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_dict.py —— 将 ECDICT 格式的 CSV 词表转换为「背单词插件」所需的词库 SQLite 数据库。

ECDICT CSV 表头（13 字段）：
    word,phonetic,definition,translation,pos,collins,oxford,tag,bnc,frq,exchange,detail,audio

生成的 .db 结构见 docs/词库格式说明.md。

用法示例：
    # 直接转换整个 csv
    python3 make_dict.py input.csv output.db --name "高考+四级"

    # 只挑选带 gk 或 cet4 标签的词条
    python3 make_dict.py ecdict.csv gk_cet4.db --name "高考四级" --tag-filter gk cet4
"""

import argparse
import csv
import os
import sqlite3
import sys
import time

# 词库表结构（与插件 C++ 端 DictDatabase 严格对应）
SCHEMA = """
CREATE TABLE IF NOT EXISTS dict_meta (
    key   TEXT PRIMARY KEY,
    value TEXT
);

CREATE TABLE IF NOT EXISTS words (
    id          INTEGER PRIMARY KEY,
    word        TEXT NOT NULL,
    phonetic    TEXT,
    definition  TEXT,
    translation TEXT,
    pos         TEXT,
    collins     INTEGER,
    oxford      INTEGER,
    tag         TEXT,
    bnc         INTEGER,
    frq         INTEGER,
    exchange    TEXT
);

CREATE INDEX IF NOT EXISTS idx_words_word ON words(word);
"""

# ECDICT CSV 列名 -> 列索引（按标准表头顺序）
COLUMNS = ["word", "phonetic", "definition", "translation", "pos",
           "collins", "oxford", "tag", "bnc", "frq", "exchange",
           "detail", "audio"]


def to_int(value):
    """把 collins/oxford/bnc/frq 这类字段安全转成 int，空值/异常一律返回 0。"""
    if value is None:
        return 0
    value = value.strip()
    if value == "":
        return 0
    try:
        return int(value)
    except ValueError:
        return 0


def matches_tag(tag_field, tag_filter):
    """判断该词条的 tag 字段（空格分隔的标签集合）是否命中过滤集合。"""
    if not tag_filter:
        return True
    tags = set(tag_field.split())
    return any(t in tags for t in tag_filter)


def main():
    parser = argparse.ArgumentParser(
        description="将 ECDICT 格式 CSV 转换为背单词插件词库 SQLite 数据库")
    parser.add_argument("input_csv", help="输入的 ECDICT 格式 CSV 文件")
    parser.add_argument("output_db", help="输出的词库 .db 文件")
    parser.add_argument("--name", default="", help="词库显示名称（写入 dict_meta.name）")
    parser.add_argument("--description", default="", help="词库描述")
    parser.add_argument("--version", default="1.0.0", help="词库版本号")
    parser.add_argument("--tag-filter", nargs="*", default=None,
                        help="只保留 tag 含有这些标签之一的词条，如：gk cet4")
    args = parser.parse_args()

    if not os.path.isfile(args.input_csv):
        print("错误：找不到输入文件 %s" % args.input_csv, file=sys.stderr)
        return 1

    # 词表里的中文释义可能很长，放开字段长度限制
    csv.field_size_limit(2 ** 31 - 1)

    # 覆盖式生成：先删除已存在的旧库，避免残留数据
    if os.path.exists(args.output_db):
        os.remove(args.output_db)

    conn = sqlite3.connect(args.output_db)
    try:
        conn.executescript(SCHEMA)

        dict_name = args.name or os.path.splitext(os.path.basename(args.output_db))[0]

        inserted = 0
        with open(args.input_csv, newline="", encoding="utf-8") as fin:
            reader = csv.reader(fin)
            header = next(reader, None)
            if header is None:
                print("错误：CSV 文件为空", file=sys.stderr)
                return 1

            # 按表头定位各列；缺失的列回退到标准顺序索引
            index = {name: header.index(name) if name in header else
                     (COLUMNS.index(name) if COLUMNS.index(name) < len(header) else -1)
                     for name in COLUMNS}

            def cell(row, name):
                i = index[name]
                return row[i] if 0 <= i < len(row) else ""

            cur = conn.cursor()
            rows = []
            for row in reader:
                if not row:
                    continue
                word = cell(row, "word").strip()
                if not word:
                    continue
                if not matches_tag(cell(row, "tag"), args.tag_filter):
                    continue

                rows.append((
                    word,
                    cell(row, "phonetic"),
                    cell(row, "definition"),
                    cell(row, "translation"),
                    cell(row, "pos"),
                    to_int(cell(row, "collins")),
                    to_int(cell(row, "oxford")),
                    cell(row, "tag"),
                    to_int(cell(row, "bnc")),
                    to_int(cell(row, "frq")),
                    cell(row, "exchange"),
                ))

                # 分批提交，控制内存占用
                if len(rows) >= 2000:
                    cur.executemany(
                        "INSERT INTO words(word,phonetic,definition,translation,"
                        "pos,collins,oxford,tag,bnc,frq,exchange) "
                        "VALUES(?,?,?,?,?,?,?,?,?,?,?)", rows)
                    inserted += len(rows)
                    rows = []

            if rows:
                cur.executemany(
                    "INSERT INTO words(word,phonetic,definition,translation,"
                    "pos,collins,oxford,tag,bnc,frq,exchange) "
                    "VALUES(?,?,?,?,?,?,?,?,?,?,?)", rows)
                inserted += len(rows)

        # 写入词库元信息
        meta = {
            "name": dict_name,
            "description": args.description,
            "version": args.version,
            "word_count": str(inserted),
            "created_at": str(int(time.time())),
        }
        conn.executemany("INSERT OR REPLACE INTO dict_meta(key,value) VALUES(?,?)",
                         list(meta.items()))
        conn.commit()

        # 优化体积与查询
        conn.execute("VACUUM")
        conn.commit()

        print("完成：写入 %d 条词条到 %s（词库名：%s）" %
              (inserted, args.output_db, dict_name))
    finally:
        conn.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
