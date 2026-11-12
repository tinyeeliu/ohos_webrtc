#!/usr/bin/env bash
# 一键运行 XTS 测试：./run-tests.sh [clean]
# 前置条件：已安装 DevEco Studio 命令行工具（hvigor）并配置 OpenHarmony SDK。
# hvigorw 包装器存在时优先使用，否则回退到 PATH 中的 hvigor。
set -euo pipefail

cd "$(dirname "$0")"

if [ -f ./hvigorw ]; then
  HVIGOR=./hvigorw
else
  HVIGOR=hvigor
fi

if [ "${1:-}" = "clean" ]; then
  "${HVIGOR}" clean --no-daemon
fi

# 执行 entry 模块 ohosTest XTS 测试套件（List.test.ets 聚合全部用例）。
# 真机/模拟器上运行时需先通过 hdc 连接设备或使用 DevEco 测试运行器。
"${HVIGOR}" test --mode module -p module=entry@ohosTest --no-daemon
