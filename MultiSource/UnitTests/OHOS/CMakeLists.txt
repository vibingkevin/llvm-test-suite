# The peer deliberately uses the SDK compiler to exercise the Clang 15/21 ABI.
set(peer "${CMAKE_CURRENT_BINARY_DIR}/libohos-peer15.so")
add_custom_command(OUTPUT "${peer}"
  COMMAND "${OHOS_SDK}/llvm/bin/clang++" --target=aarch64-linux-ohos
    "--sysroot=${OHOS_SDK}/sysroot" -std=c++17 -O3 -fPIC -shared
    "${CMAKE_CURRENT_SOURCE_DIR}/peer.cpp" -o "${peer}"
  DEPENDS peer.cpp compat.h VERBATIM)
add_custom_target(ohos-peer15 ALL DEPENDS "${peer}")
llvm_test_run(WORKDIR %S ./libohos-peer15.so)
llvm_test_verify("%b/fpcmp" "%o" "${CMAKE_CURRENT_SOURCE_DIR}/reference_output")
list(APPEND LDFLAGS -ldl -pthread)
list(APPEND CXXFLAGS -std=c++17 -pthread)
llvm_test_executable(ohos-rom-compat main.cpp)
# dlopen's peer must resolve these public C++ type identities from the caller.
target_link_options(ohos-rom-compat PRIVATE -Wl,--export-dynamic)
add_dependencies(ohos-rom-compat ohos-peer15)
