# WebRTC M132版本适配OpenHarmony应用端SDK指导

## 简介

WebRTC (Web Real-Time Communications) 是一项实时通讯技术，它允许网络应用或者站点，在不借助中间媒介的情况下，建立浏览器之间点对点（Peer-to-Peer）的连接，实现视频流和（或）音频流或者其他任意数据的传输。接下来讲解如何能将webRTC适配到OpenHarmony系统上。

## 编译前准备

### 编译环境

- ubuntu 20.04.1
- webrtc版本（m132，commitid:afaf497805cbb502da89991c2dcd783201efdd08）
- OpenHarmony SDK版本(5.0.0及以上版本)

### 源码准备

- 安装depot_tools

  创建工作目录webrtc，cd  到自己创建的工作目录，拉取工具：

  ```shell
  #mkdir webrtc
  #cd webrtc
  #git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
  ```

  成功后，将工具路径添加到环境变量中：

  ```shell
  #vi ~/.bashrc                                    # 在.bashrc文件的最后添加下面一行代码
  #export PATH="$PATH:/xxx/webrtc/depot_tools"     # 路径一定要是绝对路径


  #source ~/.bashrc                                # 配置完环境变量后使环境变量生效
  ```

- 获取代码
  
  ```shell
  #mkdir code （/xxx/webrtc/code）
  #cd code
  #fetch --nohooks webrtc
  ```

  中间如果报错，出现中断，执行： gclient sync 继续下载。没有报错，下载完后也执行一下  gclient sync。

  以上操作是下载最新全量的版本，需切换到指定的版本(m132版本对应的分支为: branch-heads/6834)：

  ```shell
  # cd src 
  # git checkout -b m132 branch-heads/6834    # 切到对应的分支
  # gclient sync -D --with_branch_heads       # 同步对应分支代码
  ```

  切完分支后可以查看对应的log：
  ![git status](./pic/webrtc_m132.png)

- 安装依赖

  ```shell
  #cd src
  #./build/install-build-deps.sh
  ```

- OpenHarmony SDK准备

  1. SDK获取：

  我们可以从可以从OpenHarmony社区获取发布对应5.0版本的SDK，如 [OpenHarmony 5.0。0 Release](https://gitee.com/openharmony/docs/blob/master/zh-cn/release-notes/OpenHarmony-v5.0.0-release.md#%E4%BB%8E%E9%95%9C%E5%83%8F%E7%AB%99%E7%82%B9%E8%8E%B7%E5%8F%96)。

  2. clang替换

  webrtc m132版本需求clang的版本最低是18，但当前OpenHarmony的SDK中的版本是15.0.4，不满足webrtc的构建需求，所以我们需要替换掉SDK中llvm。
  此处我们用已适配好的19版本替换，将[llvm 19版本](https://kotlinnativeohos.obs.cn-north-9.myhuaweicloud.com/artifact/llvm-19.1.7-x64-linux-ohos-2.tar.gz)下载并解压，将其全部替换SDK中的LLVM即可。

  ```shell
  # tar -zxvf llvm-19.1.7-x64-linux-ohos-2.tar.gz                 # 将llvm 19版本解压到本地
  # rm -rf $OHOS_SD/native/llvm/*                                 # 删除SDK中的LLVM，注意，OHOS_SDK必须是用户自己SDK的真实路径，到native上一级
  # cp -arf llvm-19.1.7-x64-linux-ohos-2/* $OHOS_SD/native/llvm/  # 将llvm 19版本拷贝到SDK目录中
  ```
  **注意：当前llvm只支持arm64以及x64架构，其他架构暂不支持。用户也可自行适配其他更高级版本的llvm。**

### 源码修改

**1. 添加OHOS平台**

```shell
diff -uprN src/build/config/BUILDCONFIG.gn src_ohos/build/config/BUILDCONFIG.gn
--- src/build/config/BUILDCONFIG.gn	2023-12-15 19:46:40.000000000 -0800
+++ src_ohos/build/config/BUILDCONFIG.gn	2023-12-18 00:17:52.679868987 -0800
@@ -235,6 +235,8 @@ _default_toolchain = ""
 if (target_os == "android") {
   assert(host_os == "linux", "Android builds are only supported on Linux.")
   _default_toolchain = "//build/toolchain/android:android_clang_$target_cpu"
+} else if (target_os == "ohos") {  # OHOS-LOCAL
+ _default_toolchain = "//build/toolchain/ohos:ohos_clang_$target_cpu"
 } else if (target_os == "chromeos" || target_os == "linux") {
   # See comments in build/toolchain/cros/BUILD.gn about board compiles.
   if (is_clang) {
@@ -298,6 +300,7 @@ if (custom_toolchain != "") {
 # aix or one of the BSDs. If you need to check these, just check the
 # current_os value directly.

+is_ohos = current_os == "ohos"
 is_android = current_os == "android"
 is_chromeos = current_os == "chromeos"
 is_fuchsia = current_os == "fuchsia"
@@ -384,6 +387,10 @@ if (is_android) {
       [ "//build/config/android:default_orderfile_instrumentation" ]
 }
 
+if (is_ohos) {
+  default_compiler_configs += [ "//build/config/ohos:compiler" ]
+}
+
 if (is_clang && !is_nacl) {
   default_compiler_configs += [
     "//build/config/clang:find_bad_constructs",
```

**2. 设置ohos平台clang 工具链相关路径**

```shell
diff -uprN src/build/config/clang/clang.gni src_ohos/build/config/clang/clang.gni
--- src/build/config/clang/clang.gni	2023-12-15 19:46:40.000000000 -0800
+++ src_ohos/build/config/clang/clang.gni	2023-12-18 01:10:11.634508775 -0800
@@ -7,14 +7,23 @@ import("//build/toolchain/toolchain.gni"
 
 default_clang_base_path = "//third_party/llvm-build/Release+Asserts"
 
+if (is_ohos) {
+  declare_args() {
+    ohos_sdk_native_root = "//buildtools/ohos-sdk/linux/native"
+  }
+  default_clang_base_path = "${ohos_sdk_native_root}/llvm"
+  clang_lib_path = "${default_clang_base_path}/lib"
+}
+
 declare_args() {
   # Indicates if the build should use the Chrome-specific plugins for enforcing
   # coding guidelines, etc. Only used when compiling with Chrome's Clang, not
   # Chrome OS's.
-  clang_use_chrome_plugins =
-      is_clang && !is_nacl && current_os != "zos" &&
-      default_toolchain != "//build/toolchain/cros:target"
-
+  if (is_ohos) {
+    clang_use_chrome_plugins = false
+  } else {
+    clang_use_chrome_plugins =
+        is_clang && !is_nacl && current_os != "zos" &&
+        default_toolchain != "//build/toolchain/cros:target"
+  }
   enable_check_raw_ptr_fields =
       build_with_chromium && !is_official_build &&
       ((is_linux && !is_castos) || (is_android && !is_cast_android) || is_mac ||
```

**特别声明：此处`ohos_sdk_native_root`配置默认的路径，用户需将ohos的SDK拷贝到buildtools目录，且目录需要和配置的保持一致，或者可以在编译时手动配置ohos sdk路径**

在clang的配置中增加OHOS对应架构路径：

```shell
diff -uprN src/build/config/clang/BUILD.gn src_ohos/build/config/clang/BUILD.gn
--- src/build/config/clang/BUILD.gn	2023-12-15 19:46:40.000000000 -0800
+++ src_ohos/build/config/clang/BUILD.gn	2023-12-18 01:10:11.634508775 -0800

    } else {
    assert(false)  # Unhandled cpu type
    }
+  } else if (is_ohos) {
+    if (current_cpu == "x64") {
+      _dir = "x86_64-linux-ohos"
+    } else if (current_cpu == "arm64") {
+      _dir = "aarch64-linux-ohos"
+    } else {
+      assert(false)  # Unhandled cpu type
+    }
  } else if (is_fuchsia) {
    if (current_cpu == "x64") {

```

**3. 设置ohos平台的sysroot**

```shell
diff -uprN src/build/config/sysroot.gni src_ohos/build/config/sysroot.gni
--- src/build/config/sysroot.gni	2023-12-15 19:46:40.000000000 -0800
+++ src_ohos/build/config/sysroot.gni	2023-12-18 00:24:42.832573268 -0800
@@ -34,6 +34,9 @@ if (sysroot == "") {
 
     # Android uses unified headers, and thus a single compile time sysroot
     sysroot = "$android_toolchain_root/sysroot"
+  } else if (is_ohos) {
+    import("//build/config/clang/clang.gni")
+    sysroot = "${ohos_sdk_native_root}/sysroot"
   } else if ((is_linux || is_chromeos) && use_sysroot) {
     # By default build against a sysroot image downloaded from Cloud Storage
     # during gclient runhooks.
```

**4. 设置ohos平台clang版本**

```shell
diff -uprN src/build/toolchain/toolchain.gni src_ohos/build/toolchain/toolchain.gni
--- src/build/toolchain/toolchain.gni	2023-12-15 19:46:40.000000000 -0800
+++ src_ohos/build/toolchain/toolchain.gni	2023-12-18 00:26:19.154065809 -0800
@@ -47,7 +47,11 @@ declare_args() {
   if (llvm_android_mainline) {  # https://crbug.com/1481060
     clang_version = "17"
   } else {
-    clang_version = "20"
+    if (is_ohos) {
+      clang_version = "19"
+    } else {
+      clang_version = "20"
+    }
   }
 }
```

**5. 扩充原本的gcc_toolchain模板功能**

这里主要是添加了ohos的用于启动引导程序的.o，gcc_toolchain.gni中template("single_gcc_toolchain")添加这几个参数的识别：

```shell
diff -uprN src/build/toolchain/gcc_toolchain.gni src_ohos/build/toolchain/gcc_toolchain.gni
--- src/build/toolchain/gcc_toolchain.gni	2023-12-15 19:46:40.000000000 -0800
+++ src_ohos/build/toolchain/gcc_toolchain.gni	2023-12-18 00:31:56.269865976 -0800
@@ -126,6 +126,30 @@ template("single_gcc_toolchain") {
       rebuild_string = ""
     }
+    if (is_ohos) {
+     if (defined(invoker.libs_section_prefix)) {
+       libs_section_prefix = invoker.libs_section_prefix
+     } else {
+       libs_section_prefix = ""
+     }
+
+     if (defined(invoker.libs_section_postfix)) {
+       libs_section_postfix = invoker.libs_section_postfix
+     } else {
+       libs_section_postfix = ""
+     }
+
+     if (defined(invoker.solink_libs_section_prefix)) {
+       solink_libs_section_prefix = invoker.solink_libs_section_prefix
+     } else {
+       solink_libs_section_prefix = ""
+     }
+
+     if (defined(invoker.solink_libs_section_postfix)) {
+       solink_libs_section_postfix = invoker.solink_libs_section_postfix
+     } else {
+       solink_libs_section_postfix = ""
+     }
+    }
+
```

修改tool("solink")和tool("solink_module")中rspfile_content：

```shell
@@ -503,7 +527,7 @@ template("single_gcc_toolchain") {
         # ld.
         rspfile_content = "{{inputs}} {{solibs}} {{libs}}"
       } else {
-        rspfile_content = "-Wl,--whole-archive {{inputs}} {{solibs}} -Wl,--no-whole-archive {{libs}}"
+        rspfile_content = "-Wl,--whole-archive {{inputs}} {{solibs}} -Wl,--no-whole-archive $solink_libs_section_prefix {{libs}} $solink_libs_section_postfix"
       }
 
       description = "SOLINK $sofile"
@@ -582,7 +606,7 @@ template("single_gcc_toolchain") {
         strip_command = "${invoker.strip} -o \"$sofile\" \"$unstripped_sofile\""
         command += " && " + strip_command
       }
-      rspfile_content = "$whole_archive_flag {{inputs}} {{solibs}} $no_whole_archive_flag {{libs}} {{rlibs}}"
+      rspfile_content = "-Wl,--whole-archive {{inputs}} {{solibs}} -Wl,--no-whole-archive $solink_libs_section_prefix {{libs}} $solink_libs_section_postfix"
 
       description = "SOLINK_MODULE $sofile"
```

修改tool("link")中link_command：

```shell
-      link_command = "$ld {{ldflags}}${extra_ldflags} -o \"$unstripped_outfile\" $start_group_flag @\"$rspfile\" $end_group_flag {{solibs}} {{libs}} $start_group_flag {{rlibs}} $end_group_flag"
+      link_command = "$ld {{ldflags}}${extra_ldflags} -o \"$unstripped_outfile\" $libs_section_prefix $start_group_flag @\"$rspfile\" {{solibs}} {{libs}} $end_group_flag  $libs_section_postfix"
```

**6. 设置各个架构的ohos_clang_toolchain**

新建build/toolchain/ohos/BUILD.gn

```shell
import("//build/config/sysroot.gni")
import("//build/toolchain/gcc_toolchain.gni")

declare_args() {
  # Whether unstripped binaries, i.e. compiled with debug symbols, should be
  # considered runtime_deps rather than stripped ones.
  ohos_unstripped_runtime_outputs = true
  ohos_extra_cflags = ""
  ohos_extra_cppflags = ""
  ohos_extra_cxxflags = ""
  ohos_extra_asmflags = ""
  ohos_extra_ldflags = ""
}

# The ohos clang toolchains share most of the same parameters, so we have this
# wrapper around gcc_toolchain to avoid duplication of logic.
#
# Parameters:
#  - toolchain_root
#      Path to cpu-specific toolchain within the ndk.
#  - sysroot
#      Sysroot for this architecture.
#  - lib_dir
#      Subdirectory inside of sysroot where libs go.
#  - binary_prefix
#      Prefix of compiler executables.
template("ohos_clang_toolchain") {
  gcc_toolchain(target_name) {
    assert(defined(invoker.toolchain_args),
           "toolchain_args must be defined for ohos_clang_toolchain()")
    toolchain_args = invoker.toolchain_args
    toolchain_args.current_os = "ohos"

    # Output linker map files for binary size analysis.
    enable_linker_map = true

    ohos_libc_dir =
        rebase_path(invoker.sysroot + "/" + invoker.lib_dir, root_build_dir)
 #   libs_section_prefix = "${ohos_libc_dir}/Scrt1.o"
 #   libs_section_prefix += " ${ohos_libc_dir}/crti.o"
 #   libs_section_postfix = "${ohos_libc_dir}/crtn.o"

    if (invoker.target_name == "ohos_clang_arm") {
      abi_target = "arm-linux-ohos"
    } else if (invoker.target_name == "ohos_clang_arm64") {
      abi_target = "aarch64-linux-ohos"
    } else if (invoker.target_name == "ohos_clang_x86_64") {
      abi_target = "x86_64-linux-ohos"
    }

    clang_rt_dir =
        rebase_path("${clang_lib_path}/${abi_target}/nanlegacy",
                    root_build_dir)
    print("ohos_libc_dir :", ohos_libc_dir)
    print("clang_rt_dir :", clang_rt_dir)
#    solink_libs_section_prefix = "${ohos_libc_dir}/crti.o"
#    solink_libs_section_prefix += " ${clang_rt_dir}/clang_rt.crtbegin.o"
#    solink_libs_section_postfix = "${ohos_libc_dir}/crtn.o"
#    solink_libs_section_postfix += " ${clang_rt_dir}/clang_rt.crtend.o"

    _prefix = rebase_path("${clang_base_path}/bin", root_build_dir)
    cc = "${_prefix}/clang"
    cxx = "${_prefix}/clang++"
    ar = "${_prefix}/llvm-ar"
    ld = cxx
    readelf = "${_prefix}/llvm-readobj"
    nm = "${_prefix}/llvm-nm"
    if (!is_debug) {
      strip = rebase_path("${clang_base_path}/bin/llvm-strip", root_build_dir)
      use_unstripped_as_runtime_outputs = ohos_unstripped_runtime_outputs
    }
    extra_cflags = ohos_extra_cflags
    extra_cppflags = ohos_extra_cppflags
    extra_cxxflags = ohos_extra_cxxflags
    extra_asmflags = ohos_extra_asmflags
    extra_ldflags = ohos_extra_ldflags
  }
}


ohos_clang_toolchain("ohos_clang_arm") {
  sysroot = "${sysroot}"
  lib_dir = "usr/lib/arm-linux-ohos"
  toolchain_args = {
    current_cpu = "arm"
  }
}

ohos_clang_toolchain("ohos_clang_arm64") {
  sysroot = "${sysroot}"
  lib_dir = "usr/lib/aarch64-linux-ohos"
  toolchain_args = {
    current_cpu = "arm64"
  }
}

ohos_clang_toolchain("ohos_clang_x86_64") {
  sysroot = "${sysroot}"
  lib_dir = "usr/lib/x86_64-linux-ohos"
  toolchain_args = {
    current_cpu = "x86_64"
  }
}

```

**7. 设置ohos的一些编译参数，将其加入到BUILDCONFIG.gn中**

新建build/config/ohos/BUILD.gn,该文件主要是定义一个config，该config会被注册到所有的编译目标，该config主要设置了基础的编译选项、宏定义等:

```shell

import("//build/config/sysroot.gni")
import("//build/config/clang/clang.gni")
assert(is_ohos)

ohos_clang_base_path = "${ohos_sdk_native_root}/llvm"
ohos_clang_version = "19"

if (is_ohos) {
  if (current_cpu == "arm64") {
    abi_target = "aarch64-linux-ohos"
  } else if (current_cpu == "x86_64") {
    abi_target = "x86_64-linux-ohos"
  } else {
    assert(false, "Architecture not supported")
  }
}


config("compiler") {
  cflags = [
    "-ffunction-sections",
    "-fno-short-enums",
    "-fno-addrsig",
  ]

  cflags += [
    "-Wno-unknown-warning-option",
    "-Wno-int-conversion",
    "-Wno-unused-variable",
    "-Wno-misleading-indentation",
    "-Wno-missing-field-initializers",
    "-Wno-unused-parameter",
    "-Wno-c++11-narrowing",
    "-Wno-unneeded-internal-declaration",
    "-Wno-undefined-var-template",
    "-Wno-implicit-int-float-conversion",
  ]
  defines = [
    # The NDK has these things, but doesn't define the constants to say that it
    # does. Define them here instead.
    "HAVE_SYS_UIO_H",
  ]

  defines += [
    "OHOS",
    "__MUSL__",
    "_LIBCPP_HAS_MUSL_LIBC",
    "__BUILD_LINUX_WITH_CLANG",
    "__GNU_SOURCE",
    "_GNU_SOURCE",
  ]

  ldflags = [
    "-Wl,--no-undefined",
    "-Wl,--exclude-libs=libunwind_llvm.a",
    "-Wl,--exclude-libs=libc++_static.a",

    # Don't allow visible symbols from libraries that contain
    # assembly code with symbols that aren't hidden properly.
    # http://crbug.com/448386
    "-Wl,--exclude-libs=libvpx_assembly_arm.a",
  ]

  cflags += [ "--target=$abi_target" ]
  include_dirs = [
    "${sysroot}/usr/include/${abi_target}",
    "${ohos_clang_base_path}/lib/clang/${ohos_clang_version}/include",
  ]

  ldflags += [ "--target=$abi_target" ]

  # Assign any flags set for the C compiler to asmflags so that they are sent
  # to the assembler.
  asmflags = cflags
}

```

**8. build/config/compiler/BUILD.gn 增加对is_ohos得判断**

```shell
@@ -1576,9 +1570,12 @@ config("compiler_deterministic") {
 config("clang_revision") {
   if (is_clang && clang_base_path == default_clang_base_path) {
     update_args = [
-      "--print-revision",
-      "--verify-version=$clang_version",
+      "--print-revision"
     ]
+
+    if (!is_ohos) {
+      update_args += [ "--verify-version=$clang_version" ]
+    }
     if (llvm_force_head_revision) {
       update_args += [ "--llvm-force-head-revision" ]
     }
```

为了避免编译中出现因警告而造成出错，需要将`-Werror`去掉

```shell
@@ -1707,12 +1704,10 @@ config("treat_warnings_as_errors") {
   if (is_win) {
     cflags = [ "/WX" ]
   } else {
-    cflags = [ "-Werror" ]
 
     # The compiler driver can sometimes (rarely) emit warnings before calling
     # the actual linker.  Make sure these warnings are treated as errors as
     # well.
-    ldflags = [ "-Werror" ]
   }
```

编译过程中会提示部分配置不识别，因此也需要将这些配置项删除：

```shell

@@ -622,14 +620,10 @@ config("compiler") {
     if (default_toolchain != "//build/toolchain/cros:target" &&
         !llvm_android_mainline) {
       cflags += [
-        "-mllvm",
-        "-split-threshold-for-reg-with-hint=0",
       ]

```

**9. 注释不支持的插件 find_bad_constructs**

注释掉BUILDCONFIG.gn中的find_bad_constructs

```shell
if (is_clang && !is_nacl) {
  default_compiler_configs += [
    "//build/config/clang:extra_warnings",
-    "//build/config/clang:find_bad_constructs",
+    #"//build/config/clang:find_bad_constructs",
    "//build/config/clang:unsafe_buffers",
  ]
}
```

在 build/nocompile.gni中注释掉find_bad_constructs

```shell
@@ -63 +63 @@ action_foreach(target_name) {
     testonly = true

     # Disable the checks that the Chrome style plugin normally enforces to
      # reduce the amount of boilerplate needed in nocompile tests.
-     configs -= [ "//build/config/clang:find_bad_constructs" ]
+     # configs -= [ "//build/config/clang:find_bad_constructs" ]

      if (is_win) {
```

在顶层目录的webrtc.gni中，对find_bad_constructs的引用排除掉ohos平台

```shell
- if (!build_with_chromium && is_clang) {
+ if (!build_with_chromium && is_clang && !is_ohos) {
    rtc_remove_configs += [ "//build/config/clang:find_bad_constructs" ]
  }
```


**10. 其他业务代码修改**

- 一些三方库没有适配ohos，可以走linux的文件，如不修改会提示部分接口找不到
  
  1. 修改libevent中的BUILD.gn

  ```shell
  diff -uprN src/third_party/libevent/BUILD.gn src_ohos/third_party/libevent/BUILD.gn
  --- src/third_party/libevent/BUILD.gn	2023-12-15 21:08:23.000000000 -0800
  +++ src_ohos/third_party/libevent/BUILD.gn	2023-12-18 00:40:20.090099934 -0800
  @@ -37,13 +37,13 @@ static_library("libevent") {
       "mac/event-config.h",
     ]
     include_dirs += [ "mac" ]
  -  } else if (is_linux || is_chromeos) {
  +  } else if (is_linux || is_chromeos || is_ohos) {
     sources += [
         "epoll.c",
         "linux/config.h",
         "linux/event-config.h",
       ]
  -    include_dirs += [ "linux" ]
  +    include_dirs += [ "linux", "compat" ]
     } else if (is_android) {
       sources += [
         "android/config.h",
  ```

  2. 修改video_capture的BUILD.gn

  ```shell
  diff -uprN src/modules/video_capture/BUILD.gn src_ohos/modules/video_capture/BUILD.gn
  --- src/modules/video_capture/BUILD.gn	2023-12-15 19:42:56.000000000 -0800
  +++ src_ohos/modules/video_capture/BUILD.gn	2023-12-18 17:43:10.824354213 -0800
  @@ -71,7 +71,7 @@ if (!build_with_chromium || is_linux ||
         "video_capture_options.h",
       ]
   
  -    if (is_linux || is_chromeos) {
  +    if (is_linux || is_chromeos || is_ohos) {
         sources += [
           "linux/device_info_linux.cc",
           "linux/device_info_v4l2.cc",
  ```
  
  3. 修改zlib的BUILD.gn

   ```shell
   diff -uprN src/third_party/zlib/BUILD.gn src_ohos/third_party/zlib/BUILD.gn
  --- src/third_party/zlib/BUILD.gn	2023-12-15 21:08:26.000000000 -0800
  +++ src_ohos/third_party/zlib/BUILD.gn	2023-12-18 00:47:21.237890949 -0800
  @@ -127,7 +127,7 @@ if (use_arm_neon_optimizations) {
       defines = [ "CRC32_ARMV8_CRC32" ]
       if (is_android) {
         defines += [ "ARMV8_OS_ANDROID" ]
  -    } else if (is_linux || is_chromeos) {
  +    } else if (is_linux || is_chromeos || is_ohos) {
         defines += [ "ARMV8_OS_LINUX" ]
       } else if (is_mac) {
         defines += [ "ARMV8_OS_MACOS" ]
   ```

  4. 修改libxml的BUILD.gn

  ```shell
   diff -uprN src/third_party/libxml/BUILD.gn src_ohos/third_party/libxml/BUILD.gn
  --- src/third_party/libxml/BUILD.gn	2023-12-15 21:08:26.000000000 -0800
  +++ src_ohos/third_party/libxml/BUILD.gn	2023-12-18 00:47:21.237890949 -0800
  
  - if (is_linux || is_chromeos || is_android || is_nacl || is_fuchsia || is_ohos) {
  + if (is_linux || is_chromeos || is_android || is_nacl || is_fuchsia || is_ohos) {
    os_include = "linux"
  } else if (is_apple) {

  ```

- 修改当前项目中不支持ohos的接口
  
  1. `rtc_base/platform_thread_types.cc`需要识别到ohos然后调用`gettid()`
  
  首先需要在根目录的BUILD.gn中配置识别ohos系统的变量：

  ```shell
  diff -uprN src/BUILD.gn src_ohos/BUILD.gn
  --- src/BUILD.gn	2023-12-15 19:42:56.000000000 -0800
  +++ src_ohos/BUILD.gn	2023-12-18 01:12:39.553298189 -0800
  @@ -214,6 +214,9 @@ config("common_inherited_config") {
         "WEBRTC_IOS",
       ]
     }
  +  if (is_ohos) {
  +    defines += [ "WEBRTC_OHOS", "WEBRTC_LINUX" ]
  +  }
     if (is_linux || is_chromeos) {
       defines += [ "WEBRTC_LINUX" ]
     }
  ```

  当前很多业务模块还未进行识别，暂时走linux分支。具体模块需要用户进行分析。

  修改rtc_base/platform_thread_types.c业务代码：
  
  ```shell
  --- src/rtc_base/platform_thread_types.cc	2023-12-15 19:42:56.000000000 -0800
  +++ src_ohos/rtc_base/platform_thread_types.cc	2023-12-18 00:42:49.393883643 -0800
  @@ -40,7 +40,7 @@ PlatformThreadId CurrentThreadId() {
   #elif defined(WEBRTC_POSIX)
   #if defined(WEBRTC_MAC) || defined(WEBRTC_IOS)
     return pthread_mach_thread_np(pthread_self());
  -#elif defined(WEBRTC_ANDROID)
  +#elif defined(WEBRTC_ANDROID) || defined(WEBRTC_OHOS)
     return gettid();
   #elif defined(WEBRTC_FUCHSIA)
     return zx_thread_self();
  ```

  当前ohos支持的函数接口为`gettid()`，因此我们可以将`WEBRTC_OHOS`判断和`WEBRTC_ANDROID`放在一起，也可以单独建一个分支，单独建分支时需要注意，其判断需要放在`WEBRTC_LINUX`之前。

  2. 屏蔽`third_party/dav1d`中不支持的接口`pthread_getaffinity_np`
  
  通过分析可知，`pthread_getaffinity_np`接口是在宏`HAVE_PTHREAD_GETAFFINITY_NP`生效时调用，故我们只需要将该宏在ohos平台时设置为不生效即可：

  ```shell
  --- src/third_party/dav1d/BUILD.gn	2023-12-15 21:08:22.000000000 -0800
  +++ src_ohos/third_party/dav1d/BUILD.gn	2023-12-18 17:39:15.988668172 -0800
  @@ -56,7 +56,7 @@ config("public_dav1d_config") {
       defines = [ "CONFIG_LOG=1" ]
     }
   
  -  if (!is_android && !is_win) {
  +  if (!is_android && !is_win && !is_ohos) {
       defines += [
         "HAVE_PTHREAD_GETAFFINITY_NP=1",
         "HAVE_PTHREAD_SETAFFINITY_NP=1",
  ```

[完整的patch文件](./webrtc_m132.patch)

## 编译

做完以上修改后，我们就可以进行编译了，默认的编译命令：

```shell
gn gen ../out/ohos_webrtc --args='is_clang=true target_cpu="arm64" target_os="ohos"'    # 通过gn生成对应的ninja文件
ninja -C ../out/ohos_webrtc -v -j32                                                     # 使用ninja进行编译
```

执行默认的编译命令后会出现以下问题：

1. 问题一：提示音频采集/解码接口找不到

![audio error](./media/audio_error.png)

通过分析代码可,该错误是在编译audio_device时出得错误，而当前webrtc已适配了android, linux, mac以及win几种平台，没有适配ohos平台，但我们也可以看到，如果将`rtc_use_dummy_audio_file_devices`该变量置起，其默认不会编audio_device相关得代码，当前可以在执行gn配置时加上参数`rtc_use_dummy_audio_file_devices=true`进行规避操作。

通过以上分析，最终的编译命令如下：

```shell
gn gen ../out/ohos_webrtc --args='is_clang=true target_cpu="arm64" target_os="ohos" rtc_use_dummy_audio_file_devices=true ohos_sdk_native_root="/owner/workspace/sdk-linux-4.1.3.500/sdk/HarmonyOS-NEXT-DP1/base/native/"'    # out目录根据实际情况设置,ohos_sdk_native_root需要配置成自己SDK的路径，配置到native目录。
ninja -C ../out/ohos_webrtc -v -j32                 # -v 查看编译的具体过程， -j32使用32个线程进行编译，提升编译速度
```

## 测试验证

按照当前步骤编译完后会生成很多测试用例，当前以`common_audio_unittests`用例上板测试验证，将`common_audio_unittests`推送到板子，后然后运行测试，效果如下图：

![test](./media/audio_unit_test1.png)
![test](./media/audio_unit_test2.png)

## 总结

通过当前方案可使用OpenHarmony的SDK编译出基本功能的webrtc，但由于采用了部分规避手段，故本方案还有几点需要注意：

1. 音频采集需要适配ohos平台。
2. 大部分的分支走linux，未精确分析。
3. 未对所有模块对ohos平台分析，当前只修改了影响编译的部分，原则上需要对所有模块进行分析。
4. 当前替换的clang只支持arm64和x64架构。
