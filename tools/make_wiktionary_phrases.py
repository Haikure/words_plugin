#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Build a words_plugin SQLite dictionary from English Wiktionary phrase entries.

The script intentionally keeps the output schema identical to tools/make_dict.py.
It filters Wiktionary category members through an existing gaokao dictionary, then
keeps entries that have Mandarin/Chinese translations in English Wiktionary.
"""

import argparse
import json
import os
import re
import sqlite3
import sys
import time
import urllib.parse
import urllib.request


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

API_URL = "https://en.wiktionary.org/w/api.php"
USER_AGENT = "words-plugin-wiktionary-builder/1.0 (https://en.wiktionary.org/)"

DEFAULT_CATEGORIES = [
    "Category:English phrasal verbs",
    "Category:English prepositional phrases",
]

STOP_HEADINGS = {
    "pronunciation", "etymology", "translations", "translation", "references",
    "further reading", "anagrams", "see also", "derived terms", "related terms",
    "synonyms", "antonyms", "coordinate terms", "hypernyms", "hyponyms",
}

POS_ABBR = {
    "verb": "v.phr.",
    "prepositional phrase": "prep.phr.",
    "phrase": "phr.",
    "proverb": "phr.",
    "idiom": "phr.",
    "adverb": "adv.phr.",
    "adjective": "adj.phr.",
    "noun": "n.phr.",
    "interjection": "interj.",
    "conjunction": "conj.",
}

PLACEHOLDER_TOKENS = {
    "one", "ones", "oneself", "someone", "somebody", "something", "somewhere",
    "anyone", "anybody", "anything", "anywhere", "everyone", "everybody",
    "everything", "else", "another",
}

BAD_TOKENS = {
    "fuck", "fucking", "shit", "shitting", "damn", "bitch", "asshole",
    "cunt", "dick", "cock", "piss", "porn", "sex",
}

UNSUITABLE_EN_RE = re.compile(
    r"\b(sex\w*|intercourse|erection\w*|erogenous|breast\w*|grope\w*|porn\w*|"
    r"prostitut\w*|masturbat\w*|orgasm\w*|ejaculat\w*|obscene|vulgar|fuck\w*|shit\w*|"
    r"have someone killed|murder\w*|kill|killed|killing|rob|robbery|steal|"
    r"stolen|thief|intoxicat\w*|drug\w*|kiss\w*|homosexual|gay)\b",
    re.IGNORECASE,
)

UNSUITABLE_ZH_RE = re.compile(
    r"(勃起|性交|上床|色情|调情|調情|吃豆腐|轻薄|輕薄|弄死|干掉|殺|杀|"
    r"卖淫|賣淫|亲热|親熱|滾|滚|毒品|吸毒)"
)

# A compact Traditional-to-Simplified map for the Chinese terms commonly found
# in Wiktionary Mandarin translation templates. It is not a full converter.
TRAD_TO_SIMP = str.maketrans({
    "愛": "爱", "礙": "碍", "罷": "罢", "辦": "办", "幫": "帮", "報": "报",
    "備": "备", "筆": "笔", "邊": "边", "變": "变", "標": "标", "別": "别",
    "並": "并", "補": "补", "參": "参", "慘": "惨", "層": "层", "產": "产",
    "場": "场", "長": "长", "車": "车", "稱": "称", "成": "成", "衝": "冲",
    "處": "处", "傳": "传", "詞": "词", "從": "从", "錯": "错", "達": "达",
    "帶": "带", "單": "单", "當": "当", "導": "导", "燈": "灯", "點": "点",
    "電": "电", "動": "动", "對": "对", "隊": "队", "頓": "顿", "發": "发",
    "飯": "饭", "煩": "烦", "範": "范", "飛": "飞", "費": "费", "紛": "纷",
    "風": "风", "負": "负", "複": "复", "復": "复", "該": "该", "幹": "干",
    "趕": "赶", "個": "个", "給": "给", "關": "关", "觀": "观", "廣": "广",
    "歸": "归", "國": "国", "過": "过", "還": "还", "漢": "汉", "號": "号",
    "後": "后", "護": "护", "劃": "划", "話": "话", "壞": "坏", "歡": "欢",
    "環": "环", "會": "会", "機": "机", "級": "级", "計": "计", "記": "记",
    "際": "际", "繼": "继", "價": "价", "間": "间", "簡": "简", "見": "见",
    "將": "将", "講": "讲", "較": "较", "節": "节", "結": "结", "進": "进",
    "經": "经", "靜": "静", "舉": "举", "據": "据", "覺": "觉", "開": "开",
    "課": "课", "塊": "块", "來": "来", "藍": "蓝", "樂": "乐", "類": "类",
    "裡": "里", "禮": "礼", "連": "连", "聯": "联", "練": "练", "兩": "两",
    "瞭": "了", "臨": "临", "領": "领", "劉": "刘", "龍": "龙", "樓": "楼",
    "錄": "录", "論": "论", "嗎": "吗", "買": "买", "賣": "卖", "門": "门",
    "們": "们", "夢": "梦", "麵": "面", "難": "难", "腦": "脑", "內": "内",
    "擬": "拟", "鳥": "鸟", "農": "农", "歐": "欧", "盤": "盘", "憑": "凭",
    "蘋": "苹", "齊": "齐", "氣": "气", "錢": "钱", "強": "强", "親": "亲",
    "輕": "轻", "請": "请", "區": "区", "權": "权", "讓": "让", "認": "认",
    "軟": "软", "賽": "赛", "傘": "伞", "喪": "丧", "掃": "扫", "設": "设",
    "聲": "声", "時": "时", "實": "实", "試": "试", "視": "视", "書": "书",
    "數": "数", "說": "说", "雖": "虽", "隨": "随", "歲": "岁", "臺": "台",
    "態": "态", "談": "谈", "題": "题", "條": "条", "聽": "听", "頭": "头",
    "圖": "图", "團": "团", "萬": "万", "網": "网", "為": "为", "維": "维",
    "衛": "卫", "謂": "谓", "溫": "温", "問": "问", "無": "无", "務": "务",
    "誤": "误", "係": "系", "習": "习", "繫": "系", "現": "现", "線": "线",
    "鄉": "乡", "響": "响", "項": "项", "寫": "写", "謝": "谢", "興": "兴",
    "須": "须", "學": "学", "尋": "寻", "訓": "训", "壓": "压", "嚴": "严",
    "驗": "验", "樣": "样", "頁": "页", "業": "业", "義": "义", "藝": "艺",
    "憶": "忆", "應": "应", "營": "营", "擁": "拥", "優": "优", "郵": "邮",
    "與": "与", "語": "语", "預": "预", "園": "园", "遠": "远", "願": "愿",
    "運": "运", "雜": "杂", "贊": "赞", "暫": "暂", "責": "责", "擇": "择",
    "戰": "战", "張": "张", "照": "照", "這": "这", "針": "针", "陣": "阵",
    "證": "证", "隻": "只", "種": "种", "眾": "众", "週": "周", "豬": "猪",
    "專": "专", "轉": "转", "裝": "装", "準": "准", "總": "总", "組": "组",
    "顧": "顾", "體": "体", "據": "据", "輯": "辑", "詞": "词",
    "續": "续", "堅": "坚", "廢": "废", "終": "终", "於": "于", "攜": "携",
    "潛": "潜", "騙": "骗", "偽": "伪", "鎮": "镇", "評": "评", "麼": "么",
    "贖": "赎", "湊": "凑", "昇": "升", "購": "购", "銷": "销", "調": "调",
    "勝": "胜", "極": "极", "盡": "尽", "儘": "尽", "幾": "几", "協": "协",
    "測": "测", "劑": "剂", "醫": "医", "藥": "药", "選": "选", "遷": "迁",
    "橋": "桥", "膽": "胆", "庫": "库", "畫": "画", "擔": "担", "勞": "劳",
    "賓": "宾", "貨": "货", "爾": "尔", "塵": "尘", "煙": "烟", "獨": "独",
    "墜": "坠", "雲": "云", "縣": "县", "灣": "湾", "員": "员", "賴": "赖",
    "遲": "迟", "餘": "余", "築": "筑", "獲": "获", "適": "适", "舊": "旧",
    "誰": "谁", "腳": "脚", "塗": "涂", "襲": "袭", "獎": "奖", "遞": "递",
    "綜": "综", "觸": "触", "險": "险", "遺": "遗", "債": "债", "鑰": "钥",
    "瑣": "琐", "獻": "献", "離": "离", "賊": "贼",
    "厲": "厉", "鬧": "闹", "編": "编", "製": "制", "滾": "滚", "違": "违",
    "軌": "轨", "虧": "亏", "賠": "赔", "蝕": "蚀", "駕": "驾", "陳": "陈",
    "創": "创", "絕": "绝", "戲": "戏", "戶": "户", "確": "确", "規": "规",
    "盜": "盗", "賭": "赌", "癮": "瘾",
    "慮": "虑", "討": "讨", "懷": "怀", "竄": "窜", "驟": "骤", "晝": "昼",
})


def api_get(params):
    query = urllib.parse.urlencode(params)
    req = urllib.request.Request(API_URL + "?" + query,
                                 headers={"User-Agent": USER_AGENT})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.loads(resp.read().decode("utf-8"))


def load_level_words(db_path):
    conn = sqlite3.connect(db_path)
    try:
        rows = conn.execute("SELECT word, frq FROM words").fetchall()
    finally:
        conn.close()

    vocab = set()
    frq = {}
    for word, rank in rows:
        word = (word or "").strip().lower()
        if re.fullmatch(r"[a-z]+(?:'[a-z]+)?", word):
            vocab.add(word)
            frq[word] = int(rank or 0)
    return vocab, frq


def category_members(category, limit):
    titles = []
    cont = {}
    while True:
        params = {
            "action": "query",
            "list": "categorymembers",
            "cmtitle": category,
            "cmnamespace": "0",
            "cmlimit": "500",
            "format": "json",
        }
        params.update(cont)
        data = api_get(params)
        titles.extend(item["title"] for item in data.get("query", {}).get("categorymembers", []))
        if limit and len(titles) >= limit:
            return titles[:limit]
        if "continue" not in data:
            return titles
        cont = data["continue"]


def phrase_tokens(title):
    return re.findall(r"[a-z]+(?:'[a-z]+)?", title.lower())


def normalize_title(title):
    return re.sub(r"\s+", " ", title.replace("_", " ").strip()).lower()


def is_candidate(title, vocab, min_words, max_words, min_known_ratio):
    phrase = normalize_title(title)
    if not re.fullmatch(r"[a-z][a-z' -]*(?: [a-z][a-z' -]*)+", phrase):
        return False
    if any(tok in phrase_tokens(phrase) for tok in BAD_TOKENS):
        return False
    tokens = phrase_tokens(phrase)
    if len(tokens) < min_words or len(tokens) > max_words:
        return False
    if len(set(tokens)) == 1:
        return False

    known = 0
    checked = 0
    for token in tokens:
        if token in PLACEHOLDER_TOKENS:
            continue
        checked += 1
        if token in vocab:
            known += 1
    if checked == 0:
        return False
    return known / checked >= min_known_ratio


def candidate_score(title, category_index, frq):
    phrase = normalize_title(title)
    ranks = []
    unknown = 0
    for token in phrase_tokens(phrase):
        if token in PLACEHOLDER_TOKENS:
            continue
        rank = frq.get(token, 0)
        if rank > 0:
            ranks.append(rank)
        else:
            unknown += 1
            ranks.append(50000)
    avg_rank = sum(ranks) / max(1, len(ranks))
    length_penalty = abs(len(phrase_tokens(phrase)) - 3) * 1200
    unknown_penalty = unknown * 10000
    return category_index * 5000 + avg_rank + length_penalty + unknown_penalty


def limit_candidates_by_category(candidates, max_pages):
    candidates = sorted(candidates, key=lambda item: (item["score"], item["title"]))
    if not max_pages or len(candidates) <= max_pages:
        return candidates

    groups = {}
    for item in candidates:
        groups.setdefault(item["category_index"], []).append(item)

    selected = []
    used = set()
    per_category = max(1, max_pages // max(1, len(groups)))
    for category_index in sorted(groups):
        for item in groups[category_index][:per_category]:
            selected.append(item)
            used.add(item["title"])

    if len(selected) < max_pages:
        for item in candidates:
            if item["title"] in used:
                continue
            selected.append(item)
            used.add(item["title"])
            if len(selected) >= max_pages:
                break

    return sorted(selected[:max_pages], key=lambda item: (item["score"], item["title"]))


def batch_pages(titles):
    for i in range(0, len(titles), 50):
        chunk = titles[i:i + 50]
        params = {
            "action": "query",
            "prop": "revisions",
            "titles": "|".join(chunk),
            "rvprop": "content",
            "rvslots": "main",
            "format": "json",
            "formatversion": "2",
        }
        data = api_get(params)
        for page in data.get("query", {}).get("pages", []):
            revisions = page.get("revisions") or []
            if not revisions:
                continue
            slots = revisions[0].get("slots", {})
            main = slots.get("main", {})
            yield page.get("title", ""), main.get("content", "")


def english_section(text):
    match = re.search(r"(?m)^==English==\s*$", text)
    if not match:
        return ""
    rest = text[match.end():]
    next_lang = re.search(r"(?m)^==[^=].*==\s*$", rest)
    return rest[:next_lang.start()] if next_lang else rest


def template_arg(match, index=2):
    parts = match.group(1).split("|")
    return parts[index] if len(parts) > index else ""


def clean_wiki_text(text):
    text = re.sub(r"<!--.*?-->", "", text)
    text = re.sub(r"<ref[^>]*>.*?</ref>", "", text, flags=re.DOTALL)
    text = re.sub(r"<[^>]+>", "", text)
    text = re.sub(r"'''?", "", text)
    text = re.sub(r"\[\[([^|\]]+)\|([^]]+)\]\]", r"\2", text)
    text = re.sub(r"\[\[([^]]+)\]\]", r"\1", text)

    def replace_template(match):
        body = match.group(1)
        parts = body.split("|")
        name = parts[0].strip().lower()
        if name in {"l", "m", "mention", "link", "term"} and len(parts) >= 3:
            return parts[2]
        if name in {"gloss", "non-gloss definition"} and len(parts) >= 2:
            return parts[1]
        if name in {"lb", "label", "tlb", "qualifier", "q", "senseid", "defdate"}:
            return ""
        if name in {"ux", "uxi", "quote-book", "quote-text", "quote-journal"}:
            return ""
        return ""

    previous = None
    while previous != text:
        previous = text
        text = re.sub(r"\{\{([^{}]+)\}\}", replace_template, text)

    text = text.replace("&quot;", '"').replace("&amp;", "&")
    text = re.sub(r"\s+", " ", text).strip(" ;,")
    return text


def is_unsuitable_definition(text):
    return bool(UNSUITABLE_EN_RE.search(text))


def is_unsuitable_translation(text):
    return bool(UNSUITABLE_ZH_RE.search(text))


def extract_definitions(section):
    definitions = []
    current_heading = ""
    for line in section.splitlines():
        heading = re.match(r"^={3,5}\s*([^=]+?)\s*={3,5}\s*$", line)
        if heading:
            current_heading = heading.group(1).strip()
            continue
        if not line.startswith("# ") or line.startswith(("#*", "#:", "##")):
            continue
        heading_key = current_heading.lower()
        if heading_key in STOP_HEADINGS:
            continue
        raw = line[2:].strip()
        if "alternative form of" in raw.lower() or "misspelling of" in raw.lower():
            continue
        cleaned = clean_wiki_text(raw)
        if not cleaned or len(cleaned) < 8:
            continue
        if cleaned.lower().startswith("used other than figuratively or idiomatically"):
            continue
        if is_unsuitable_definition(cleaned):
            continue
        abbr = POS_ABBR.get(heading_key, POS_ABBR.get(heading_key.split(",")[0], "phr."))
        item = "%s %s" % (abbr, cleaned)
        if item not in definitions:
            definitions.append(item)
        if len(definitions) >= 4:
            break
    return definitions


def extract_pos(section, category):
    heading_pos = []
    for line in section.splitlines():
        heading = re.match(r"^={3,5}\s*([^=]+?)\s*={3,5}\s*$", line)
        if not heading:
            continue
        key = heading.group(1).strip().lower()
        if key in POS_ABBR:
            heading_pos.append(POS_ABBR[key])
    if heading_pos:
        return " ".join(dict.fromkeys(heading_pos))
    if "prepositional" in category.lower():
        return "prep.phr."
    if "phrasal verb" in category.lower():
        return "v.phr."
    return "phr."


def strip_translation_markup(term):
    term = clean_wiki_text(term)
    term = re.sub(r"\s*\([^)]*\)\s*$", "", term)
    term = term.strip(" ;,")
    return term.translate(TRAD_TO_SIMP)


def extract_translations(section):
    translations = []
    in_translations = False
    depth = 0
    for line in section.splitlines():
        heading = re.match(r"^(={3,5})\s*([^=]+?)\s*\1\s*$", line)
        if heading:
            title = heading.group(2).strip().lower()
            if title == "translations":
                in_translations = True
                depth = len(heading.group(1))
                continue
            if in_translations and len(heading.group(1)) <= depth:
                in_translations = False
        if not in_translations:
            continue
        for match in re.finditer(r"\{\{t[+\-a-z]*\|(cmn|zh)\|([^|{}]+)(?:\|[^{}]*)?\}\}", line):
            term = strip_translation_markup(match.group(2))
            if term and not is_unsuitable_translation(term) and term not in translations:
                translations.append(term)
        for match in re.finditer(r"\{\{zh-l\|([^|{}]+)(?:\|[^{}]*)?\}\}", line):
            term = strip_translation_markup(match.group(1))
            if term and not is_unsuitable_translation(term) and term not in translations:
                translations.append(term)
    return translations


def build_rows(args):
    vocab, frq = load_level_words(args.level_db)

    seen = {}
    for cat_index, category in enumerate(args.category):
        print("Fetching category: %s" % category, file=sys.stderr)
        for title in category_members(category, args.max_members_per_category):
            phrase = normalize_title(title)
            if phrase in seen:
                continue
            if not is_candidate(phrase, vocab, args.min_words, args.max_words, args.min_known_ratio):
                continue
            seen[phrase] = {
                "title": title,
                "category": category,
                "category_index": cat_index,
                "score": candidate_score(phrase, cat_index, frq),
            }

    candidates = limit_candidates_by_category(seen.values(), args.max_pages)
    print("Candidate pages after level filter: %d" % len(candidates), file=sys.stderr)

    rows = []
    by_title = {item["title"]: item for item in candidates}
    fetched = 0
    for title, text in batch_pages([item["title"] for item in candidates]):
        fetched += 1
        item = by_title.get(title)
        if not item:
            continue
        section = english_section(text)
        if not section:
            continue
        translations = extract_translations(section)
        if not translations:
            continue
        definitions = extract_definitions(section)
        if not definitions:
            continue

        phrase = normalize_title(title)
        translation = "\\n".join(translations[:4])
        definition = "\\n".join(definitions[:4])
        pos = extract_pos(section, item["category"])
        rows.append({
            "word": phrase,
            "phonetic": "",
            "definition": definition,
            "translation": translation,
            "pos": pos,
            "collins": 0,
            "oxford": 0,
            "tag": args.tag,
            "bnc": 0,
            "frq": 0,
            "exchange": "",
            "score": item["score"],
        })
        if args.target and len(rows) >= args.target:
            break
        if fetched % 100 == 0:
            print("Fetched %d pages, kept %d entries" % (fetched, len(rows)), file=sys.stderr)

    rows.sort(key=lambda row: (row["score"], row["word"]))
    for index, row in enumerate(rows, start=1):
        row["frq"] = index
    return rows


def write_db(path, rows, args):
    if os.path.exists(path):
        os.remove(path)

    conn = sqlite3.connect(path)
    try:
        conn.executescript(SCHEMA)
        conn.executemany(
            "INSERT INTO words(word,phonetic,definition,translation,pos,"
            "collins,oxford,tag,bnc,frq,exchange) VALUES(?,?,?,?,?,?,?,?,?,?,?)",
            [
                (
                    row["word"], row["phonetic"], row["definition"],
                    row["translation"], row["pos"], row["collins"],
                    row["oxford"], row["tag"], row["bnc"], row["frq"],
                    row["exchange"],
                )
                for row in rows
            ],
        )
        meta = {
            "name": args.name,
            "description": args.description,
            "version": args.version,
            "word_count": str(len(rows)),
            "created_at": str(int(time.time())),
        }
        conn.executemany("INSERT OR REPLACE INTO dict_meta(key,value) VALUES(?,?)",
                         list(meta.items()))
        conn.commit()
        conn.execute("VACUUM")
        conn.commit()
    finally:
        conn.close()


def main():
    parser = argparse.ArgumentParser(
        description="Build a words_plugin dictionary from English Wiktionary phrases.")
    parser.add_argument("output_db", help="Output SQLite dictionary path")
    parser.add_argument("--level-db", default="dicts/gaokao.db",
                        help="Existing words_plugin DB used as the level vocabulary filter")
    parser.add_argument("--category", action="append", default=None,
                        help="Wiktionary category to scan; may be repeated")
    parser.add_argument("--target", type=int, default=500,
                        help="Stop after keeping this many entries; 0 means no target")
    parser.add_argument("--max-pages", type=int, default=1800,
                        help="Maximum candidate pages to fetch after filtering; 0 means all")
    parser.add_argument("--max-members-per-category", type=int, default=0,
                        help="Maximum category members to scan per category; 0 means all")
    parser.add_argument("--min-words", type=int, default=2)
    parser.add_argument("--max-words", type=int, default=5)
    parser.add_argument("--min-known-ratio", type=float, default=0.8,
                        help="Minimum ratio of phrase tokens present in --level-db")
    parser.add_argument("--tag", default="gk")
    parser.add_argument("--name", default="Wiktionary gaokao phrases")
    parser.add_argument("--description", default=(
        "English Wiktionary phrasal verbs and prepositional phrases filtered by gaokao vocabulary; "
        "Mandarin translations extracted from English Wiktionary."))
    parser.add_argument("--version", default="1.0.0")
    args = parser.parse_args()

    if args.category is None:
        args.category = DEFAULT_CATEGORIES
    if args.max_pages == 0:
        args.max_pages = None
    if args.max_members_per_category == 0:
        args.max_members_per_category = None

    if not os.path.isfile(args.level_db):
        print("Missing level DB: %s" % args.level_db, file=sys.stderr)
        return 2

    rows = build_rows(args)
    if not rows:
        print("No entries were kept.", file=sys.stderr)
        return 1

    write_db(args.output_db, rows, args)
    print("Wrote %d entries to %s" % (len(rows), args.output_db))
    return 0


if __name__ == "__main__":
    sys.exit(main())
