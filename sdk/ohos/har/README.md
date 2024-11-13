编译生成的libohos_webrtc.so请放置在ohos_webrtc/libs目录下，其中预先编译好的arm64-v8a版本动态库使用如下参数编译：
    is_clang=true target_os="ohos" target_cpu="arm64"

armeabi-v7a版本的动态库使用如下参数编译：
    is_clang=true target_os="ohos" target_cpu="arm" arm_float_abi="softfp"