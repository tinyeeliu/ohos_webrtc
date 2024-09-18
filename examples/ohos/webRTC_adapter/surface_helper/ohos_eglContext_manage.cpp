/*
# Copyright (c) 2024 Huawei Device Co., Ltd.
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
*/
#include "ohos_eglContext_manage.h"

namespace webrtc {
namespace ohos {

OhosEGLContextManage OhosEGLContextManage::ohosEGLContextManage_;

bool OhosEGLContextManage::AddEGLContext(OHOSVideoBuffer::VideoSourceType sourceType, EGLContext eglContext)
{
    std::lock_guard<std::mutex> lock(mapMutex_);
    // 判断sourceType是否已经添加过eglContext
    auto iter = eglContextMap_.find(sourceType);
    if (iter != eglContextMap_.end()) {
        return false;
    }

    eglContextMap_[sourceType] = std::make_shared<EGLContextResource>(eglContext);
    return true;
}

std::shared_ptr<EGLContextResource> OhosEGLContextManage::GetEGLContextResource(
    OHOSVideoBuffer::VideoSourceType sourceType)
{
    std::lock_guard<std::mutex> lock(mapMutex_);
    auto iter = eglContextMap_.find(sourceType);
    if (iter == eglContextMap_.end()) {
        return nullptr;
    }

    return iter->second;
}

bool OhosEGLContextManage::DelEGLContext(OHOSVideoBuffer::VideoSourceType sourceType)
{
    eglContextMap_.erase(sourceType);
    return true;
}

} // namespace ohos
} // namespace webrtc