# 单词记忆插件 words_plugin

适用于 PenMods 的单词记忆插件

---

## 功能

- **多词库**：一库一文件，启动自动扫描 `dicts/`，可随时切换；各词库进度互不干扰
- **学习**：按当代词频优先选新词，过程中随机穿插已学词复习， 每轮学完自动进入「轮次巩固」
- **复习**：看英文选中文 / 看中文选英文 混合，四选一回答
- **智能复习**：初次答错连续再现 2 次；连对 ≥3 次清除高错标记；结合艾宾浩斯间隔 1 / 2 / 4 / 7 / 15 / 30 天调度
- **断点恢复**：随时退出，重进弹「上次有未完成的学习，是否继续？」（继续 / 重新开始）
- **主页**：今日学/复词数、连续打卡天数、当前词库总进度、待复习数
- **每轮数量**：5 / 10 / 15 / 20 / 25 / 30 可选

---

## 目录结构

```
words_plugin/
├── xmake.lua              # 构建配置
├── metadata.json          # 插件元数据
├── README.md              # 本文件
├── docs/
│   └── dict_introduction.md  # 词库 DB 结构文档（供生成自定义词库）
├── tools/
│   ├── make_dict.py       # 示例ECDICT CSV → 词库 .db 转换脚本
│   └── check_dict.py      # 词库 DB 检查工具，检查是否符合要求
├── src/                   # C++ 源码
│   ├── plugin.cpp         # 插件入口（init/attach/destroy）
│   ├── Sqlite.*           # sqlite3 RAII 封装
│   ├── DictDatabase.*     # 词库（只读）访问
│   ├── UserDatabase.*     # 用户库（读写）：进度/复习/统计
│   ├── StudySession.*     # 学习会话
│   ├── ReviewSession.*    # 复习会话
│   └── WordController.*   # QML 门面单例
├── qml/                   # UI界面
│   ├── main.qml           # 根页面 + 路由 + 续学弹窗
│   ├── Theme.qml          # 配色/字体
│   ├── HomePage.qml / DictSelectPage.qml / StudyPage.qml / ReviewPage.qml / SummaryPage.qml
│   └── components/        # ProgressBar / StatTile / OptionButton / WordCard / PillButton
└── dicts/                 # 词库（内含预设词库）
    ├── gaokao.db          # 高考词汇 3677
    ├── cet4.db            # 四级词汇 3849
    └── ecdict_gk_cet4.db  # 高考+四级 5331
```

---

## 构建

依赖：[Qt 开发框架](https://github.com/Redbeanw44602/aarch64-linux-qt-5.15.2)、`libsqlite3`。

```bash
# 配置xmake
xmake f --qt="<your_qt_path>" --arch=arm64-v8a --toolchain=zig --cross=aarch64-linux-gnu.2.27 -m release -vD
# 构建
xmake
```

> 注意：需使用 zig 编译 sqlite3 并放置 `include`、`lib`到项目目录下的 sqlite3 目录中

---

## 部署

部署后目录至少包含：

```
words_plugin/
├── libword_plugin.so
├── metadata.json
├── qml/
└── dicts/
```

`userdata/user.db`（学习进度、设置、统计）会在插件首次运行时自动创建，**勿手动删除**，否则进度丢失。

---

## 词库管理

本项目自带示例词库，来自 [ECDICT](https://github.com/skywind3000/ECDICT)

### 添加自有词库

任何符合 `docs/dict_introduction.md` 结构的 `.db` 放进 `dicts/` 即被识别。
词库的中文显示名写在库内 `dict_meta` 表的 `name` 键。

### 用脚本从 ECDICT CSV 生成

```bash
# 整表
python3 tools/make_dict.py input.csv dicts/mybook.db --name "我的词库"

# 仅指定考试标签（如六级）
python3 tools/make_dict.py ecdict.csv dicts/cet6.db --name "六级" --tag-filter cet6
```

ECDICT 常见标签：`zk` 中考 · `gk` 高考 · `cet4` 四级 · `cet6` 六级 · `ky` 考研 ·
`toefl` · `ielts` · `gre`。

### 生成自定义转换脚本

把 `docs/dict_introduction.md` 连同你的词表样例交给 AI，让它生成
「你的词表 → 本插件词库」的转换脚本。

---

## 数据存储

| 数据 | 位置 | 读写 |
| ---- | ---- | ---- |
| 词库 | `dicts/*.db` | 只读 |
| 进度/复习/设置/统计 | `userdata/user.db` | 读写，运行时生成 |

学习状态按 `dict_id`（词库文件名）区分，切换词库不会丢失各自进度。
