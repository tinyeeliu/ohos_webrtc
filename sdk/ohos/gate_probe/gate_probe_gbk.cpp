/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 */
// 门禁规则验证：本文件故意使用 GBK 编码保存，用于触发非 UTF-8 编码检查规则
#include <cstdint>

int32_t ProbeGbkComment(int32_t input)
{
    // 中文注释：这一行是 GBK 编码，不是 UTF-8
    return input;
}
