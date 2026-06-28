#!/bin/bash
set -euo pipefail

# 获取脚本所在目录并进入
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"
mkdir words_plugin
cp -r qml/ words_plugin/
cp icon.png words_plugin/
cp build/linux/arm64-v8a/release/libwords_plugin.so words_plugin/
cp metadata.json words_plugin/
cp -r dicts/ words_plugin/
zip -r words_plugin.zip words_plugin/
rm -r words_plugin/
echo "打包完成"
