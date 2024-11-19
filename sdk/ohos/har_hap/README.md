此目录为OpenHarmony应用示例工程，其中ohos_webrtc模块使用编译生成的`libohos_webrtc.so`及相关接口文件构建HAR包以供entry模块使用。

`ohos_webrtc/libs`目录包含预先编译好的`libohos_webrtc.so`，其中`arm64-v8a`版本编译参数为：
``` shell
gn gen out/arm64 --args='is_clang=true target_os="ohos" target_cpu="arm64" ohos_extra_ldflags="-static-libstdc++" is_official_build=true ohos_sdk_native_root="path/to/ohos/sdk"'
```

`armeabi-v7a`版本编译参数为：
``` shell
gn gen out/arm32 --args='is_clang=true target_os="ohos" target_cpu="arm" arm_float_abi="softfp" ohos_extra_ldflags="-static-libstdc++" is_official_build=true ohos_sdk_native_root="path/to/ohos/sdk"'
```

**说明**
> 如果不指定`ohos_extra_ldflags="-static-libstdc++"`参数，则需要将sdk中的`libc++_shared.so`一起放到`ohos_webrtc/libs`目录下。