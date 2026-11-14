
if(DEFINED CMAKE_ANDROID_NDK)
    set(ANDROID_NDK "${CMAKE_ANDROID_NDK}" CACHE PATH "Android NDK Path" FORCE)
endif()

list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES 
    CMAKE_ANDROID_NDK 
    CUSTOM_LLVM_DIR 
    ANDROID_ABI 
    ANDROID_PLATFORM
    CMAKE_SYSTEM_NAME
    CMAKE_SYSTEM_VERSION
    CMAKE_ANDROID_ARCH_ABI
    CUSTOM_LIBCXX_DIR
)

set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_ANDROID_ARCH_ABI ${ANDROID_ABI})

set(ANDROID_STL "none" CACHE STRING "Disable NDK STL" FORCE)
set(CMAKE_ANDROID_STL_TYPE "none" CACHE STRING "Disable CMake native STL" FORCE)

if(CMAKE_HOST_WIN32)
    set(NDK_HOST_TAG "windows-x86_64")
elseif(CMAKE_HOST_APPLE)
    set(NDK_HOST_TAG "darwin-x86_64")
else()
    set(NDK_HOST_TAG "linux-x86_64")
endif()

set(CMAKE_SYSROOT "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${NDK_HOST_TAG}/sysroot")

if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(TARGET_TRIPLE "aarch64-linux-android34")
    set(BUILTIN_ARCH "aarch64")
endif()

if(NOT DEFINED CUSTOM_LLVM_DIR)
    message(FATAL_ERROR "CUSTOM_LLVM_DIR not defined!")
endif()

set(CMAKE_C_COMPILER "${CUSTOM_LLVM_DIR}/clang.exe" CACHE FILEPATH "C compiler" FORCE)
set(CMAKE_CXX_COMPILER "${CUSTOM_LLVM_DIR}/clang++.exe" CACHE FILEPATH "CXX compiler" FORCE)
set(CMAKE_C_COMPILER_CLANG_SCAN_DEPS "${CUSTOM_LLVM_DIR}/clang-scan-deps.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER_CLANG_SCAN_DEPS "${CUSTOM_LLVM_DIR}/clang-scan-deps.exe" CACHE FILEPATH "" FORCE)

if(NOT DEFINED CUSTOM_LIBCXX_DIR)
    set(CUSTOM_LIBCXX_DIR "C:/miasma/androidlibcxx")
endif()

set(LIBCXX_LIB_DIR "${CUSTOM_LIBCXX_DIR}/lib")
if(EXISTS "${CUSTOM_LIBCXX_DIR}/lib/aarch64-linux-android")
    set(LIBCXX_LIB_DIR "${CUSTOM_LIBCXX_DIR}/lib/aarch64-linux-android")
elseif(EXISTS "${CUSTOM_LIBCXX_DIR}/lib/aarch64-unknown-linux-android")
    set(LIBCXX_LIB_DIR "${CUSTOM_LIBCXX_DIR}/lib/aarch64-unknown-linux-android")
endif()

set(CMAKE_C_FLAGS_INIT "--no-default-config -fPIC -ftls-model=global-dynamic")
set(CMAKE_CXX_FLAGS_INIT "--no-default-config -fPIC -ftls-model=global-dynamic")

set(CMAKE_CXX_STANDARD_LIBRARIES_INIT "-l:libc++.a -l:libc++abi.a -latomic -l:libunwind.a -ldl -lm -lc")
set(CMAKE_C_STANDARD_LIBRARIES_INIT "-latomic -l:libunwind.a -ldl -lm -lc")

set(NDK_CLANG_VERSION "21")
set(NDK_CLANG_LINUX_DIR "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${NDK_HOST_TAG}/lib/clang/${NDK_CLANG_VERSION}/lib/linux")
set(NDK_CLANG_ARCH_DIR "${NDK_CLANG_LINUX_DIR}/aarch64")
set(NDK_SYSROOT_LIB_DIR "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${NDK_HOST_TAG}/sysroot/usr/lib/aarch64-linux-android")
set(NDK_SYSROOT_API_DIR "${NDK_SYSROOT_LIB_DIR}/34")

set(CUSTOM_LINKER_PATH "${CUSTOM_LLVM_DIR}/ld.lld.exe")

set(ALL_CUSTOM_L_FLAGS "-fuse-ld=\"${CUSTOM_LINKER_PATH}\" -Wl,--threads=1 -mno-fix-cortex-a53-843419 -L\"${LIBCXX_LIB_DIR}\" -L\"${NDK_CLANG_LINUX_DIR}\" -L\"${NDK_CLANG_ARCH_DIR}\" -L\"${NDK_SYSROOT_API_DIR}\" -L\"${NDK_SYSROOT_LIB_DIR}\"")

set(CMAKE_EXE_LINKER_FLAGS_INIT "${ALL_CUSTOM_L_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${ALL_CUSTOM_L_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${ALL_CUSTOM_L_FLAGS}")

execute_process(
    COMMAND "${CUSTOM_LLVM_DIR}/clang.exe" --print-resource-dir
    OUTPUT_VARIABLE CLANG_RESOURCE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
file(TO_CMAKE_PATH "${CLANG_RESOURCE_DIR}" CLANG_RESOURCE_DIR)

set(NDK_BUILTINS_FILE "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${NDK_HOST_TAG}/lib/clang/${NDK_CLANG_VERSION}/lib/linux/libclang_rt.builtins-${BUILTIN_ARCH}-android.a")

if(NOT DEFINED CMAKE_C_COMPILER_TARGET)
    set(COMPILER_TARGET_DIR "${TARGET_TRIPLE}")
else()
    set(COMPILER_TARGET_DIR "${CMAKE_C_COMPILER_TARGET}")
endif()

set(CLANG22_BUILTINS_DIR "${CLANG_RESOURCE_DIR}/lib/${COMPILER_TARGET_DIR}")
set(CLANG22_BUILTINS_FILE "${CLANG22_BUILTINS_DIR}/libclang_rt.builtins.a")

if(EXISTS "${NDK_BUILTINS_FILE}" AND NOT EXISTS "${CLANG22_BUILTINS_FILE}")
    message(STATUS "Proxy Toolchain: Synthesizing Clang runtimes for ${COMPILER_TARGET_DIR}...")
    file(MAKE_DIRECTORY "${CLANG22_BUILTINS_DIR}")
    file(COPY_FILE "${NDK_BUILTINS_FILE}" "${CLANG22_BUILTINS_FILE}")
endif()
