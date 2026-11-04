# OHOS ROM 兼容性测试

目的：用 Kotlin v2.4.21 所用的 JetBrains LLVM21 编译，使用 Command Line Tools 中的 DevEco15 系统头、libc++ 头和链接资源，在 OHOS 真机加载 ROM 的共享库。不要向设备推送 SDK 的 libc++，不要设置指向 SDK 库的 LD_LIBRARY_PATH 或 rpath。

对齐依据：Kotlin `test/jb2420-minimal-ohos-native` 的 `dd6510e66db95dbf6f8f06264bbda5d5787bb989`，特别是 `ClangArgs.Native`、native 测试的 `compileWithClang` 和 `OhosLinker`。编译和链接都使用 SDK 的 `-resource-dir=.../15.0.4`。测试自身可选择 C/C++ 标准、优化等级和预处理开关；这些是用例参数。没有给所有第三方测试强加 KN 测试默认的 `-fmodules`。

本分支基于 llvm-test-suite `llvmorg-21.1.6`。HDC 执行器在配套 LLVM 分支的 `libcxx/utils/hdc.py`。

## 配置、部署、运行

先设置绝对路径：`OHOS_SDK` 指向 `command-line-tools/sdk/default/openharmony/native`；`LLVM_HOME` 指向 JB LLVM21；`LLVM_SOURCE` 指向配套 LLVM 源码；`BUILD` 指向本会话的独立构建目录；`DEVICE` 是完整 HDC 设备 ID；`REMOTE` 是本会话独占的 `/data/local/tmp/` 子目录。设备按工作区规则使用租约。

```sh
cmake -S . -B "$BUILD" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/toolchains/ohos.cmake" \
  -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_C_FLAGS_RELEASE=-O3 -DSMALL_PROBLEM_SIZE' \
  '-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DSMALL_PROBLEM_SIZE' \
  -DSMALL_PROBLEM_SIZE=ON \
  '-DTEST_SUITE_SUBDIRS=SingleSource;MultiSource' \
  -DTEST_SUITE_COPY_DATA=ON -DTEST_SUITE_USER_MODE_EMULATION=ON \
  -DTEST_SUITE_COLLECT_CODE_SIZE=OFF -DTEST_SUITE_COLLECT_COMPILE_TIME=OFF \
  "-DTEST_SUITE_RUN_UNDER=python3 $LLVM_SOURCE/libcxx/utils/hdc.py --device $DEVICE --remote-root $REMOTE --local-root $BUILD --no-sync --merge-stderr --timeout 1200 --audit $BUILD/device-executions.jsonl --"
ninja -C "$BUILD" -j2 -k0
python3 utils/ohos-stage.py "$BUILD" --device "$DEVICE" --remote-root "$REMOTE"
cd "$BUILD"
python3 "$LLVM_SOURCE/llvm/utils/lit/lit.py" -v -j4 -o results.json .
```

必须同时指定 CMake 的 `SMALL_PROBLEM_SIZE` 和编译宏，否则部分程序用大数据集运行，却与小数据集输出比较。`TEST_SUITE_USER_MODE_EMULATION` 使 timeit/fpcmp 在 host 构建；测试 ELF 由 HDC 在设备执行，不使用 QEMU。必须从构建根目录调用 lit，GCC torture 的非传统格式不会另设工作目录。

O0 回归使用独立构建目录，`-O0`，不设置小数据集；可用 `SingleSource/UnitTests;SingleSource/Regression;MultiSource/UnitTests`。直接选择子目录时，要把 `SingleSource/lit.local.cfg` 和 `MultiSource/lit.local.cfg` 复制到构建树对应父目录，否则输出验证格式错误。

`ohos-stage.py` 上传测试及其数据，建立测试需要的空 Output 目录。HDC 执行器映射命令中的本地绝对路径，回传真实退出码；传统 test-suite 合并 stdout/stderr，保持原始输出顺序。断言或程序校验在设备进行，输出比较和 lit 结果判定在 host 进行。计时包含传输等开销，不能用于性能结论。

## 跨编译器 ABI 探针

`MultiSource/UnitTests/OHOS` 用 SDK Clang15 构建共享库，以 LLVM21 构建调用程序，检查 RTTI、双向异常、跨库字符串和 new/delete、线程局部存储与原子。`OHOS_COMPAT_TRACE=1` 可打印实际映射库，核实加载 ROM libc++。共享的类型信息必须从主程序导出。

## 结果口径

构建失败不能用旧产物替代；失败及跳过需保留用例名、原始诊断、平台/标准要求及 Clang15 对照。语言前端、代码生成、FileCheck 等纯 host 测试不作为 ROM 兼容性覆盖。缺少外部商业数据集的 External 组另列，不算通过。

已确认原 setup 不能编译包含 DevEco15 `arm_neon.h` 的 LLVM21 程序。禁止改用 LLVM21 intrinsic 头后把结果计入原 setup。AArch64 FMV 用例也需要单独核对 OHOS/musl 的 resolver 支持。

## 本轮 O3 验证边界

2026-11-04，JB LLVM21 `c2e420df942a21f93c15248b4fb9259737fa58c1`，Command Line Tools Clang15.0.4 / SDK26，AArch64 HarmonyOS 6.1.0.120、API24 真机：SingleSource + MultiSource 共 2029 项，2022 PASS、4 输出校验失败、3 构建失败。所有成功构建的 2026 个程序均实际执行；ELF 依赖只有 ROM 提供的 `libc++_shared.so`、`libc.so`，未设置 rpath。SDK Clang15 构建的 ABI peer 是测试自身的共享库。

| 非成功用例 | 实测结论 |
| --- | --- |
| Vector/NEON/Vector-NEON-simple；Vector/AArch64/aarch64_neon_intrinsics | LLVM21 不接受 SDK15 arm_neon.h 使用的旧 builtin 签名；SDK15 编译相同程序并在同一设备通过。是原 setup 的真实编译限制，不能当通过或合理跳过。 |
| AArch64/aarch64-acle-fmv-features | LLVM21 暴露 FMV 宏，但 OHOS/musl 不支持对应 ifunc resolver，链接缺少未绑定的默认入口；SDK builtins 也缺新的 CPU feature helper。SDK15 未开启该宏，不能把其 fallback 算作 FMV 验证。 |
| ClamAV/clamscan | 合并输出顺序与上游参考不同，程序退出 0；Clang15/21 的同机输出逐字节一致。 |
| obsequi/Obsequi；Prolangs-C++/city；Ptrdist/ft | 用例依赖 libc random() 序列，ROM 的搜索轨迹、路径及图摘要与上游参考不同；Clang15/21 的同机输出逐字节一致。保留 FAIL，不用新输出覆盖原参考。 |

已修复的用例适配包括：无 `/tmp` 时使用测试工作目录；缺 gettext/getpass 时选择应用已有回退路径；补全平台头和 SMALL_PROBLEM_SIZE；catalog 打开失败后不调用 catclose(-1)。`consumer-typeset` 的崩溃经设备 LLDB 确认为 catclose(-1)，修复后通过。长循环用例扩大执行时限后通过，不作为数值差异处理。

这些结果验证当前 ROM 上的兼容范围，不能证明所有 ROM 版本或所有 SDK API 均兼容，也不能声称原 setup 无条件正确。External 的外部数据集、CUDA/Fortran 等额外工具链，以及 LLVM 的 host FileCheck 测试不在上述 2029 项中。
