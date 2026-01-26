#!/bin/bash

# Copyright (c) 2023 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

function check_depot_tools(){
    local tmp=`echo $PATH | grep "depot_tool"`
    if [ -z "$tmp" ]
    then
        return 1
    fi
    return 0
}

function download_thirdparty() {  # 修复原脚本中的拼写错误（downlaod -> download）
    if [ -d "third_party" ]
    then
        return 0
    fi

    git clone https://gitee.com/zhong-luping/webrtc_third_party.git
    if [ $? -ne 0 ]
    then
        echo "git clone webrtc third_party failed!!"
        return 1
    fi

    mv webrtc_third_party/third_party ./
    rm -rf webrtc_third_party
    mv -f third_party_libjpeg_turbo/BUILD.gn third_party/libjpeg_turbo/BUILD.gn
    return 0
}

# 默认SDK路径（用户可自行修改或通过参数传入）
OHOS_SDK_NATIVE_PATH=/data/webrtc/ohos-sdk-windows_linux-public/linux/native-linux-x64-6.0.0.47-Beta1/native
if [ ! -z "$1" ]
then
    OHOS_SDK_NATIVE_PATH="$1"
fi
if [ ! -d "$OHOS_SDK_NATIVE_PATH" ]
then
    echo "[ERROR] can't find the OHOS_SDK_NATIVE_PATH: $OHOS_SDK_NATIVE_PATH"
    echo "Usage: ./build_x86_64.sh [OHOS_SDK_NATIVE_PATH]"
    exit 1
fi

# 同步子模块（原生命令，若无需可注释）
git submodule update --init --recursive

# 检查depot_tools是否在PATH中
check_depot_tools
if [ $? -ne 0 ]
then
    echo "[ERROR] webrtc build depends on depot_tools! Please add depot_tools to PATH first."
    exit 1
fi

# 下载第三方依赖
download_thirdparty
if [ $? -ne 0 ]
then
    echo "[ERROR] Failed to prepare webrtc third_party!"
    exit 1
fi

# 构建参数：重点修改 target_cpu 为 "x64"（WebRTC中x86_64的标准写法）
 arguments="target_cpu=\"x64\" \
    target_os=\"ohos\" \
    ohos_sdk_native_root=\"$OHOS_SDK_NATIVE_PATH\" \
    ohos_extra_ldflags=\"-static-libstdc++\" \
    is_official_build=true \
    is_clang=true"

# 生成x86_64架构的构建目录（out/webrtc_x86_64，避免与arm64目录冲突）
gn gen out/webrtc_x86_64 --args="$arguments"
if [ $? -ne 0 ]
then
    echo "[ERROR] Failed to generate webrtc x86_64 config!"
    exit 1
fi

# 编译（输出日志到build_x86_64.log）
ninja -C out/webrtc_x86_64 -v -j32 > build_x86_64.log 2>&1
if [ $? -ne 0 ]
then
    echo "[ERROR] webrtc x86_64 build failed! Check details in build_x86_64.log"
    exit 1
fi

echo "### webrtc x86_64 build success! Output in out/webrtc_x86_64 ###"
