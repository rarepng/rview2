cmake_minimum_required(VERSION 4.0)
project(rview2 LANGUAGES CXX)

include(ExternalProject)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)

find_package(Vulkan REQUIRED)

set(OPENSSL_USE_STATIC_LIBS ON)
set(ZLIB_USE_STATIC_LIBS ON)
find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

file(DOWNLOAD
     https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/get_cpm.cmake
     ${CMAKE_CURRENT_BINARY_DIR}/cpm.cmake
)
include(${CMAKE_CURRENT_BINARY_DIR}/cpm.cmake)


set(submodules_path
        ${PROJECT_SOURCE_DIR}/sub
)

find_program(ASTYLE_EXECUTABLE astyle)
if(ASTYLE_EXECUTABLE)
    file(GLOB_RECURSE ALL_SOURCE_FILES 
        "src/*.cpp" 
        "include/*.hpp" 
        "mods/*.cxxm"
    )
    add_custom_target(format
        COMMAND ${ASTYLE_EXECUTABLE} --options=${CMAKE_SOURCE_DIR}/.astylerc ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "astyling..."
    )
endif()

if(UNIX)
set(SHELL_CMD bash)
set(SEP ":")
set(STATIC_LIB_EXT a)
else()
set(SHELL_CMD pwsh)  #powershell 7
#set(SHELL_CMD powershell) #default for windows 
set(EXT ".exe")
set(SEP ";")
set(STATIC_LIB_EXT lib)
endif()

add_definitions(-DSTEAMNETWORKINGSOCKETS_STATIC_LINK=1)
add_definitions(-DABSL_OPTION_USE_STD_ORDERING=0)

if(UNIX)
    set(GNS_LINKER_FLAGS "-static -lstdc++ -lz -lpthread -ldl")
else()
    set(GNS_LINKER_FLAGS "-static -lws2_32 -lcrypt32 -lstdc++")
endif()

if(NOT CMAKE_BUILD_TYPE STREQUAL "clanged")
ExternalProject_Add(abslEX
    SOURCE_DIR ${submodules_path}/absl
    CMAKE_ARGS
        ${GLOBAL_DEFAULT_ARGS}
        -G Ninja
        -DBUILD_TESTING=OFF
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/cmake_more_cache/absl
        -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}
        -DBUILD_SHARED_LIB=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DABSL_OPTION_USE_STD_ORDERING=0
        -DCMAKE_CXX_FLAGS='-D\ ABSL_OPTION_USE_STD_ORDERING=0\ -D\ ABSL_USES_STD_ORDERING=0'
        -DCMAKE_C_FLAGS='-D\ ABSL_OPTION_USE_STD_ORDERING=0\ -D\ ABSL_USES_STD_ORDERING=0'
        -DCMAKE_CXX_FLAGS_DEBUG="-g"
        # -DCMAKE_EXE_LINKER_FLAGS='-static'
        -B ${PROJECT_SOURCE_DIR}/cmake_more_cache/absl_build
    BUILD_COMMAND ninja
    INSTALL_COMMAND ninja install
)


ExternalProject_Add(protoEX
    SOURCE_DIR ${submodules_path}/protobuf
    CMAKE_ARGS
        ${GLOBAL_DEFAULT_ARGS}
        -G Ninja
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
        -Dprotobuf_BUILD_TESTS=OFF
        -Dprotobuf_BUILD_SHARED_LIBS=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto
        -DABSL_OPTION_USE_STD_ORDERING=0
        -DCMAKE_CXX_FLAGS='-I${PROJECT_SOURCE_DIR}/cmake_more_cache/absl/include'
        -DCMAKE_C_FLAGS='-I${PROJECT_SOURCE_DIR}/cmake_more_cache/absl/include'
        # -DCMAKE_EXE_LINKER_FLAGS='-static'
        -DCMAKE_CXX_FLAGS_DEBUG="-g"
        -Dprotobuf_WITH_ZLIB=OFF
        -Dprotobuf_ABSL_PROVIDER=package
        -DCMAKE_PREFIX_PATH=${PROJECT_SOURCE_DIR}/cmake_more_cache/absl/lib/cmake/absl
        -DABSL_ROOT_DIR=${PROJECT_SOURCE_DIR}/cmake_more_cache/absl/lib/cmake/absl
        -DINSTALL_GTEST=OFF
        -Dprotobuf_BUILD_EXAMPLES=OFF
        -B ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto_build
    BUILD_COMMAND ninja
    INSTALL_COMMAND ninja install
)


ExternalProject_Add(gnsEX
    SOURCE_DIR ${submodules_path}/gns
    CMAKE_ARGS
        ${GLOBAL_DEFAULT_ARGS}
        -G Ninja
        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DOPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
        -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD}
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
        -DProtobuf_USE_STATIC_LIBS=ON
        -Dprotobuf_BUILD_TESTS=OFF
        -Dprotobuf_BUILD_SHARED_LIBS=OFF
        -DOPENSSL_USE_STATIC_LIBS=ON
        -DZLIB_USE_STATIC_LIBS=ON
        -DOPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE_DIR}
        -DOPENSSL_CRYPTO_LIBRARY=${OPENSSL_CRYPTO_LIBRARY}
        -DOPENSSL_SSL_LIBRARY=${OPENSSL_SSL_LIBRARY}
        
        # 2. Bypass the failing OpenSSL version check:
        -DHAVE_EVP_MD_CTX_FREE=1
        
        # 3. Provide dependencies for any other internal static link checks GNS might run:
        -DCMAKE_REQUIRED_LIBRARIES=dl;pthread
        -DProtobuf_LIBRARIES=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/lib/libprotobuf.a
        -DProtobuf_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/include
        -DBUILD_SHARED_LIB=OFF
        -DBUILD_STATIC_LIB=ON
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_STATIC_LIBS=ON
        -DProtobuf_PROTOC_EXECUTABLE=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/bin/protoc${EXT}
        -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/cmake_more_cache/gns
        -DCMAKE_PREFIX_PATH=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/bin
        -DProtobuf_DIR=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto
        -DSTEAMNETWORKINGSOCKETS_STATIC_LINK=1
        "-DCMAKE_EXE_LINKER_FLAGS=${GNS_LINKER_FLAGS}"
        # "-DCMAKE_EXE_LINKER_FLAGS=-static\ -L${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/lib\ -L${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/absl\ -lprotobuf\ -lprotoc\ -lutf8_range\ -labsl_synchronization\ -labsl_symbolize\ -labsl_strings\ -labsl_str_format_internal\ -labsl_strings_internal\ -labsl_log_initialize\ -labsl_log_entry\ -labsl_log_flags\ -labsl_log_severity\ -labsl_log_internal_check_op\ -labsl_log_internal_conditions\ -labsl_log_internal_message\ -labsl_log_internal_nullguard\ -labsl_log_internal_proto\ -labsl_log_internal_format\ -labsl_log_internal_globals\ -labsl_log_internal_log_sink_set\ -labsl_log_sink\ -labsl_raw_logging_internal\ -labsl_log_globals\ -labsl_cord\ -labsl_cordz_info\ -labsl_cordz_handle\ -labsl_cordz_functions\ -labsl_cord_internal\ -labsl_crc_cord_state\ -labsl_crc32c\ -labsl_crc_internal\ -labsl_exponential_biased\ -labsl_synchronization\ -labsl_time\ -labsl_time_zone\ -labsl_int128\ -labsl_examine_stack\ -labsl_stacktrace\ -labsl_symbolize\ -labsl_demangle_internal\ -labsl_debugging_internal\ -labsl_malloc_internal\ -labsl_throw_delegate\ -labsl_strerror\ -labsl_raw_hash_set\ -labsl_hash\ -labsl_city\ -labsl_low_level_hash\ -labsl_base\ -labsl_spinlock_wait\ -labsl_status\ -std=c++20\ -lstdc++"
        #-DCMAKE_CXX_FLAGS='-D\ STEAMNETWORKINGSOCKETS_STATIC_LINK=1\ --static\ -static\ -I${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/include\ -I${PROJECT_SOURCE_DIR}/cmake_more_cache/absl/include\ -L${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/lib\ -L${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/absl\ -lprotobuf\ -lprotoc\ -lutf8_range\ -labsl_synchronization\ -labsl_symbolize\ -labsl_strings\ -labsl_str_format_internal\ -labsl_strings_internal\ -labsl_log_initialize\ -labsl_log_entry\ -labsl_log_flags\ -labsl_log_severity\ -labsl_log_internal_check_op\ -labsl_log_internal_conditions\ -labsl_log_internal_message\ -labsl_log_internal_nullguard\ -labsl_log_internal_proto\ -labsl_log_internal_format\ -labsl_log_internal_globals\ -labsl_log_internal_log_sink_set\ -labsl_log_sink\ -labsl_raw_logging_internal\ -labsl_log_globals\ -labsl_cord\ -labsl_cordz_info\ -labsl_cordz_handle\ -labsl_cordz_functions\ -labsl_cord_internal\ -labsl_crc_cord_state\ -labsl_crc32c\ -labsl_crc_internal\ -labsl_exponential_biased\ -labsl_synchronization\ -labsl_time\ -labsl_time_zone\ -labsl_int128\ -labsl_examine_stack\ -labsl_stacktrace\ -labsl_symbolize\ -labsl_demangle_internal\ -labsl_debugging_internal\ -labsl_malloc_internal\ -labsl_throw_delegate\ -labsl_strerror\ -labsl_raw_hash_set\ -labsl_hash\ -labsl_city\ -labsl_low_level_hash\ -labsl_base\ -labsl_spinlock_wait\ -labsl_status\ -std=c++20\ -lstdc++'
        "-DCMAKE_CXX_FLAGS=-L${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/lib\ -L${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/absl\ -I${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/include\ -I${PROJECT_SOURCE_DIR}/cmake_more_cache/absl/include"
        # \ -L${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/lib\ -L${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/absl\ -lprotobuf-lite\ -lprotobuf\ -lprotoc\ -lutf8_range\ -std=c++20\ -lstdc++\ -lpthread\ -lupb\ -lutf8_validity'
        -B "${PROJECT_SOURCE_DIR}/cmake_more_cache/gns_build"
    BUILD_COMMAND ninja
    INSTALL_COMMAND ninja install
    DEPENDS protoEX
)

endif()


set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "set" FORCE)


set(BUILD_SHARED_LIBS OFF CACHE BOOL "set" FORCE)


# will have to add the rest later..
set(FASTGLTF_INSTALL OFF CACHE BOOL "no install for fastgltf" FORCE)
set(VkBootstrap_INSTALL OFF CACHE BOOL "no vkb install either" FORCE)

set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)

CPMAddPackage(
    NAME glm
    GITHUB_REPOSITORY g-truc/glm
    GIT_TAG 1.0.1
    DOWNLOAD_ONLY YES
    EXCLUDE_FROM_ALL YES
)
# if(glm_ADDED)
    add_library(glm INTERFACE)
    target_include_directories(glm SYSTEM INTERFACE ${glm_SOURCE_DIR})
# endif()

CPMAddPackage(
    NAME simdjson
    GITHUB_REPOSITORY simdjson/simdjson
    GIT_TAG v4.6.4
    EXCLUDE_FROM_ALL YES
)

CPMAddPackage(
    NAME volk
    GITHUB_REPOSITORY zeux/volk
    GIT_TAG 1.4.350
    EXCLUDE_FROM_ALL YES
)

CPMAddPackage(
    NAME sdl
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-3.4.4
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "SDL_SHARED OFF"
        "SDL_STATIC ON"
        "SDL_TESTS OFF"
)

CPMAddPackage(
    NAME sdlimg
    GITHUB_REPOSITORY libsdl-org/SDL_image
    GIT_TAG release-3.4.4
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "SDLSHARED OFF"
        "SDLSTATIC ON"
        "SDL3IMAGE_SAMPLES OFF"
        "SDL3IMAGE_TESTS OFF"
)

CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG master
    EXCLUDE_FROM_ALL YES
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME stb
    GITHUB_REPOSITORY nothings/stb
    GIT_TAG master
    EXCLUDE_FROM_ALL YES
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME tracy
    GITHUB_REPOSITORY wolfpld/tracy
    GIT_TAG v0.13.1
    EXCLUDE_FROM_ALL YES
    OPTIONS 
        "TRACY_ENABLE ON"
)

set(CMAKE_SKIP_INSTALL_RULES ON)
add_subdirectory(${submodules_path}/fastgltf EXCLUDE_FROM_ALL)
add_subdirectory(${submodules_path}/vk_boot EXCLUDE_FROM_ALL)
add_subdirectory(${submodules_path}/vma EXCLUDE_FROM_ALL)
set(CMAKE_SKIP_INSTALL_RULES OFF)

# rip
target_compile_options(vk-bootstrap PRIVATE -mno-avx512f -mno-avx2 -mno-avx)
target_compile_definitions(simdjson PUBLIC SIMDJSON_IMPLEMENTATION_ICELAKE=0 SIMDJSON_IMPLEMENTATION_HASWELL=0)


file(GLOB_RECURSE PROJECT_HEADERS "include/*.hpp")
file(GLOB PROJECT_SOURCES "src/main.cpp" "src/*/*.cpp")
set(OTHER_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_BUILD_TYPE STREQUAL "x") # clangd

    add_library(rview2 ${OTHER_SOURCES} ${PROJECT_SOURCES} ${PROJECT_HEADERS})


    target_include_directories(rview2 PUBLIC
        "${Vulkan_INCLUDE_DIRS}"
        "${PROJECT_SOURCE_DIR}/cmake_more_cache/gns/include/GameNetworkingSockets"
        "${PROJECT_SOURCE_DIR}/cmake_more_cache/gns/include"
        "${PROJECT_SOURCE_DIR}/cmake_more_cache/absl/include"
        "${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/include"
        "${submodules_path}/vma/include"
        "${submodules_path}/vk_boot/src"
        "${submodules_path}/fastgltf/include"
        "${stb_SOURCE_DIR}"
        "${imgui_SOURCE_DIR}"
        "${tracy_SOURCE_DIR}/public"
        "${submodules_path}/jolt"
        "${submodules_path}/meshoptimizer/src"
        "${PROJECT_SOURCE_DIR}/include"
    )

    target_precompile_headers(rview2 PUBLIC
        <vector>
        <string>
        <memory>
        <iostream>
        <algorithm>
        <optional>
        <functional>
    )

else()
add_executable(rview2 ${OTHER_SOURCES} ${PROJECT_SOURCES} ${PROJECT_HEADERS})

target_compile_definitions(rview2 PRIVATE
"RVIEW_DEBUG=$<IF:$<CONFIG:Debug>,1,0>"
)

target_link_directories(rview2 PRIVATE
${PROJECT_SOURCE_DIR}/cmake_more_cache/gns/lib
${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/lib
${PROJECT_SOURCE_DIR}/cmake_more_cache/absl/lib
#${OPENSSL_ROOT}/lib64
)

add_dependencies(protoEX abslEX)
add_dependencies(gnsEX protoEX)
add_dependencies(rview2 gnsEX)


target_link_libraries(rview2
    PRIVATE
        "$<$<CONFIG:Debug>:TracyClient>"
        SDL3-static
        SDL3_image-static
        fastgltf
        glm
        Vulkan::Vulkan
        vk-bootstrap::vk-bootstrap
        "-Wl,--start-group"
        GameNetworkingSockets_s
        OpenSSL::SSL
        OpenSSL::Crypto
        "$<$<CONFIG:Debug>:protobufd>"
        "$<$<NOT:$<CONFIG:Debug>>:protobuf>"
        "$<$<CONFIG:Debug>:protocd>"
        "$<$<NOT:$<CONFIG:Debug>>:protoc>"
        "$<$<CONFIG:Debug>:upbd>"
        "$<$<NOT:$<CONFIG:Debug>>:upb>"
        "$<$<CONFIG:Debug>:protobuf-lited>"
        "$<$<NOT:$<CONFIG:Debug>>:protobuf-lite>"
        utf8_range
        utf8_validity

        absl_base
        absl_city
        absl_civil_time
        absl_cord
        absl_cord_internal
        absl_cordz_functions
        absl_cordz_handle
        absl_cordz_info
        absl_cordz_sample_token
        absl_crc_cord_state
        absl_crc_cpu_detect
        absl_crc_internal
        absl_crc32c
        absl_examine_stack                    ################################################
        absl_symbolize                        ##                                            ##
        absl_demangle_internal                ##            DO NOT CHANGE ORDER             ##
        absl_demangle_rust                    ##                    !!!                     ##
        absl_debugging_internal               ##                                            ##
        absl_decode_rust_punycode             ################################################
        absl_die_if_null
        absl_exponential_biased
        absl_failure_signal_handler
        absl_flags_commandlineflag
        absl_flags_commandlineflag_internal
        absl_flags_config
        absl_flags_internal
        absl_flags_marshalling
        absl_flags_parse
        absl_flags_private_handle_accessor
        absl_flags_program_name
        absl_flags_reflection
        absl_flags_usage
        absl_flags_usage_internal
        absl_graphcycles_internal
        absl_hash
        absl_hashtablez_sampler
        absl_int128
        absl_kernel_timeout_internal
        absl_leak_check
        # absl_log_entry
        absl_log_flags
        absl_log_globals
        absl_log_initialize
        absl_log_internal_check_op
        absl_log_internal_conditions
        absl_log_internal_fnmatch
        absl_log_internal_format
        absl_log_internal_globals
        absl_log_internal_log_sink_set
        absl_log_internal_message
        absl_log_internal_nullguard
        absl_log_internal_proto
        absl_log_internal_structured_proto
        absl_log_severity
        absl_log_sink
        absl_malloc_internal
        absl_periodic_sampler
        absl_poison
        absl_random_distributions
        absl_random_internal_distribution_test_util
        absl_random_internal_platform
        # absl_random_internal_pool_urbg
        absl_random_internal_randen
        absl_random_internal_randen_hwaes
        absl_random_internal_randen_hwaes_impl
        absl_random_internal_randen_slow
        absl_random_internal_seed_material
        absl_random_seed_gen_exception
        absl_random_seed_sequences
        absl_raw_hash_set
        absl_raw_logging_internal
        absl_scoped_set_env
        absl_spinlock_wait
        absl_stacktrace
        absl_status
        absl_statusor
        absl_str_format_internal
        absl_strerror
        absl_string_view
        absl_strings
        absl_strings_internal
        absl_synchronization
        absl_throw_delegate
        absl_time
        absl_time_zone
        absl_utf8_for_code_point
        absl_vlog_config_internal
        "-Wl,--end-group"
)
if(UNIX)
target_link_libraries(rview2
    PRIVATE
        c
        pthread
)
else()
target_link_libraries(rview2
    PRIVATE
        #pthread
        #ws2_32                               #windows
        DbgHelp                               #windows
        winmm                                 #windows
        iphlpapi                              #windows
)
endif()




endif()


target_include_directories(rview2
    BEFORE
    PUBLIC
        "${PROJECT_SOURCE_DIR}/cmake_more_cache/gns/include/GameNetworkingSockets"
        "${stb_SOURCE_DIR}"
        "${imgui_SOURCE_DIR}"
        "${simdjson_SOURCE_DIR}/include"
        "${tracy_SOURCE_DIR}/public"
        "${submodules_path}/vma/include"
        "${submodules_path}/jolt"
        "${submodules_path}/meshoptimizer/src"
        "${PROJECT_SOURCE_DIR}/include"
)



#worst decision so far
# target_sources(rview2
#     PUBLIC
#         FILE_SET CXX_MODULES 
#         FILES
#             mods/tex.cxxm
# )

target_compile_options(rview2 PUBLIC
    "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>,$<CONFIG:sanitize>>:-g>"
    "$<$<CONFIG:Debug>:-Wall;-Wextra;-Wunused-variable>"
    "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:Distribution>,$<CONFIG:BleedingEdge>,$<CONFIG:MinSizeRel>>:-O3;-DNDEBUG>"
    "$<$<CONFIG:BleedingEdge>:-march=native>"
    "$<$<CONFIG:Distribution>:-march=x86-64-v3>"
    "$<$<CONFIG:sanitize>:-O1;-fno-omit-frame-pointer;-fsanitize=address>"
    # "$<$<CONFIG:RelWithDebInfo>:-mstackrealign>"
    # "$<$<CONFIG:Release>:-march=native>"
    # way too slow
    # "$<$<CONFIG:Release>:-mstackrealign>"
    "$<$<CONFIG:clanged>:-std=c++26;-fmodules;-fcxx-modules;-fno-implicit-modules;-fno-implicit-module-maps;-Wno-module-import-in-extern-c>"
    "$<$<CONFIG:mermaid>:-g;-fdump-lang-class>"

    # temporary
    "$<$<CONFIG:Debug>:-Wno-unused-parameter>"
    "$<$<CONFIG:Debug>:-Wno-missing-field-initializers>"

)
target_link_options(rview2 PUBLIC
    "$<$<CONFIG:sanitize>:-static-libasan>"
    "$<$<CONFIG:sanitize>:-fsanitize=address>"
)


set(SLANG_COMMON_FLAGS
    -target spirv 
    -capability SPIRV_1_6
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-g>
    $<$<OR:$<CONFIG:Release>,$<CONFIG:Distribution>,$<CONFIG:BleedingEdge>,$<CONFIG:MinSizeRel>>:-O3>
)


# consider only statically -static-libgcc -static-libstdc++ idk
target_link_options(rview2 PRIVATE
    # "$<$<OR:$<CONFIG:Release>,$<CONFIG:Distribution>,$<CONFIG:BleedingEdge>,$<CONFIG:MinSizeRel>>:-static>"
    # "$<$<OR:$<CONFIG:Release>,$<CONFIG:Distribution>,$<CONFIG:BleedingEdge>,$<CONFIG:MinSizeRel>>:-static-libstdc++>"
    # "$<$<OR:$<CONFIG:Release>,$<CONFIG:Distribution>,$<CONFIG:BleedingEdge>,$<CONFIG:MinSizeRel>>:-static-libgcc>"
    # no idea here..
    # "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>,$<CONFIG:sanitize>>:-static>"
    "$<$<AND:$<PLATFORM_ID:Windows>,$<OR:$<CONFIG:Distribution>,$<CONFIG:Release>,$<CONFIG:BleedingEdge>,$<CONFIG:MinSizeRel>>>:-mwindows>"
)

# wip wip
# manually adding shaders for now
function(add_shader)
    set(options "")
    set(oneValueArgs SOURCE ENTRY STAGE OUTPUT_NAME)
    set(multiValueArgs "")
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(SHADER_SOURCE "${PROJECT_SOURCE_DIR}/shaders/${ARG_SOURCE}")
    set(SPV_OUTPUT "${PROJECT_BINARY_DIR}/shaders/${ARG_OUTPUT_NAME}")
    get_filename_component(OUT_DIR ${SPV_OUTPUT} DIRECTORY)
    file(MAKE_DIRECTORY ${OUT_DIR})
    add_custom_command(
        OUTPUT "${SPV_OUTPUT}"
        COMMAND slangc
            -entry ${ARG_ENTRY}
            -stage ${ARG_STAGE}
            ${SLANG_COMMON_FLAGS}
            "${SHADER_SOURCE}"
            -o "${SPV_OUTPUT}"
            -depfile "${SPV_OUTPUT}.d"
        DEPENDS "${SHADER_SOURCE}"
        DEPFILE "${SPV_OUTPUT}.d"
        VERBATIM
        COMMAND_EXPAND_LISTS
        COMMENT "compiling sader: ${ARG_SOURCE} (${ARG_ENTRY})"
    )
    set(ALL_SHADER_SPV_FILES ${ALL_SHADER_SPV_FILES} "${SPV_OUTPUT}" PARENT_SCOPE)
endfunction()
add_shader(SOURCE red.slang ENTRY vxmain STAGE vertex OUTPUT_NAME vx.spv)
add_shader(SOURCE red.slang ENTRY pxmain STAGE pixel OUTPUT_NAME px.spv)
add_shader(SOURCE red.slang ENTRY cxmain STAGE compute OUTPUT_NAME cx.spv)
# add_shader(SOURCE dbg.slang ENTRY vxmain STAGE vertex OUTPUT_NAME vx.spv)
# add_shader(SOURCE dbg.slang ENTRY pxmain STAGE pixel OUTPUT_NAME px.spv)
# add_shader(SOURCE dbg.slang ENTRY cxmain STAGE compute OUTPUT_NAME cx.spv)
add_shader(SOURCE particle.slang ENTRY vxmain STAGE vertex OUTPUT_NAME pvx.spv)
add_shader(SOURCE particle.slang ENTRY pxmain STAGE pixel OUTPUT_NAME ppx.spv)
add_shader(SOURCE particle.slang ENTRY cxmain STAGE compute OUTPUT_NAME pcx.spv)

add_custom_target(shaders_tar ALL DEPENDS ${ALL_SHADER_SPV_FILES})
add_dependencies(rview2 shaders_tar)


add_custom_command(
    TARGET rview2 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/boot.json"
        "$<TARGET_FILE_DIR:rview2>/boot.json"
        
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${PROJECT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:rview2>/resources"
        
    COMMENT "copying resources and configs"
)





set_property(TARGET rview2 APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${PROJECT_BINARY_DIR}/resources)
set_property(TARGET rview2 APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${PROJECT_BINARY_DIR}/shaders)


install(TARGETS rview2 RUNTIME DESTINATION . CONFIGURATIONS Release Distribution BleedingEdge MinSizeRel)
install(FILES ${ALL_SHADER_SPV_FILES} DESTINATION shaders)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/" DESTINATION resources)
install(FILES "${PROJECT_SOURCE_DIR}/boot.json" DESTINATION .)

if(WIN32)
    get_filename_component(RVIEW2_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

    install(CODE "
        message(STATUS \"resolving dynamic libraries for $<TARGET_FILE_NAME:rview2>...\")
        
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES \"$<TARGET_FILE:rview2>\"
            DIRECTORIES \"${RVIEW2_BIN_DIR}\"
            RESOLVED_DEPENDENCIES_VAR RESOLVED_DEPS
            UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED_DEPS
            PRE_EXCLUDE_REGEXES 
                \"api-ms-.*\" 
                \"ext-ms-.*\" 
                \".*[Ss]ystem32.*\" 
                \".*[Ss]ys[Ww][Oo][Ww]64.*\" 
                \".*[Ww]in[Ss]x[Ss].*\"
        )

        foreach(DEP \${RESOLVED_DEPS})
            get_filename_component(DEP_NAME \${DEP} NAME)
            string(TOLOWER \${DEP_NAME} LOWER_DEP_NAME)
            
            if(NOT LOWER_DEP_NAME MATCHES \"^(kernel32|kernelbase|user32|msvcrt|msvcp.*|advapi32|ws2_32|shell32|setupapi|ole32|oleaut32|gdi32|imm32|version|winmm|crypt32|dbghelp|iphlpapi|vulkan-1|ntdll|combase|sechost|rpcrt4|win32u|cfgmgr32)\\\\.dll$\")
                message(STATUS \"deploying \${DEP_NAME}\")
                file(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}\" TYPE SHARED_LIBRARY FILES \"\${DEP}\")
            endif()
        endforeach()
    "
    CONFIGURATIONS Release Distribution BleedingEdge MinSizeRel)
    # maybe ill setup a launcher pattern
endif()

