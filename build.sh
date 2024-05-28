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
#!/bin/sh

function check_depot_tools(){
    local tmp=`echo $PATH | grep "depot_tool"`
    if [ -z $tmp ]
    then
        return 1
    fi

    return 0
}

function downlaod_thirdparty() {

    if [ -d third_party ]
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

    return 0
}

OHOS_SDK_NATIVE_PATH=/mnt/sdcard/workspace/sdk-linux-4.1.3.500/sdk/HarmonyOS-NEXT-DP1/base/native   # 用户需配置自己SDK路径
if [ ! -z $1 ]
then
    OHOS_SDK_NATIVE_PATH=$1
fi
if [ ! -d $OHOS_SDK_NATIVE_PATH ]
then
    echo "[ERROR] can't find the OHOS_SDK_NATIVE_PATH, you must config the OHOS_SDK_NATIVE_PATH first!"
    echo "use ./build.sh OHOS_SDK_NATIVE_PATH"
    exit 1
fi

check_depot_tools
if [ $? -ne 0 ]
then
    echo "[ERROR] webrtc build depends depot tools! please make sure your depot tools have been ready!!!"
    exit 1
fi

downlaod_thirdparty
if [ $? -ne 0 ]
then
    echo "[ERROR] webrtc third_party not ready! please download webrtc third party first!"
    exit 1
fi

arguments="target_cpu=\"arm64\" \
    target_os=\"ohos\" \
    ohos_sdk_native_root=\"$OHOS_SDK_NATIVE_PATH\" \
    is_clang=true"

gn gen out/webrtc --args="$arguments"
if [ $? -ne 0 ]
then
    echo "[ERROR] webrtc config faield!"
    exit 1
fi

ninja -C out/webrtc -v -j32 > build.log
if [ $? -ne 0 ]
then
    echo "[ERROR] webrtc build failed! see the detailes in build.log file"
    exit 1
fi

echo "### webrtc build success!! ###"

#eof
