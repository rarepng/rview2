cmake_minimum_required(VERSION 4.0)
project(rview2 LANGUAGES CXX)

include(ExternalProject)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)

add_compile_options(
    "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWDbg>,$<CONFIG:Sanitize>>:-g>"
    "$<$<CONFIG:Debug>:-Wall;-Wextra;-Wunused-variable>"
    "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWDbg>,$<CONFIG:Distribution>,$<CONFIG:BleedingEdge>>:-O3;-DNDEBUG>"
    "$<$<CONFIG:BleedingEdge>:-march=native>"
    "$<$<CONFIG:Distribution>:-march=x86-64-v3>"
    "$<$<CONFIG:Sanitize>:-O1;-fno-omit-frame-pointer;-fsanitize=address>"
    # "$<$<CONFIG:RelWDbg>:-mstackrealign>"
    # "$<$<CONFIG:Release>:-march=native>"
    # way too slow
    # "$<$<CONFIG:Release>:-mstackrealign>"
)
add_link_options(
    "$<$<CONFIG:Sanitize>:-static-libasan>"
    "$<$<CONFIG:Sanitize>:-fsanitize=address>"
)

find_package(Vulkan REQUIRED)

set(OPENSSL_USE_STATIC_LIBS ON)
find_package(OpenSSL REQUIRED)


set(submodules_path
        ${PROJECT_SOURCE_DIR}/sub
)

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


ExternalProject_Add(abslEX
    SOURCE_DIR ${submodules_path}/absl
    CMAKE_ARGS
        ${GLOBAL_DEFAULT_ARGS}
        -G Ninja
        -DBUILD_TESTING=OFF
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/cmake_more_cache/absl
        -DCMAKE_CXX_STANDARD=20
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
        -DCMAKE_CXX_COMPILER=g++
        -DCMAKE_C_COMPILER=gcc
        -DBUILD_SHARED_LIB=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DABSL_OPTION_USE_STD_ORDERING=0
        -DCMAKE_CXX_FLAGS='-static\ -D\ ABSL_OPTION_USE_STD_ORDERING=0\ -D\ ABSL_USES_STD_ORDERING=0'
        -DCMAKE_C_FLAGS='-static\ -D\ ABSL_OPTION_USE_STD_ORDERING=0\ -D\ ABSL_USES_STD_ORDERING=0'
        -B ${PROJECT_SOURCE_DIR}/cmake_more_cache/absl_build
    BUILD_COMMAND ninja
    INSTALL_COMMAND ninja install
)


ExternalProject_Add(protoEX
    SOURCE_DIR ${submodules_path}/protobuf
    CMAKE_ARGS
        ${GLOBAL_DEFAULT_ARGS}
        -G Ninja
        -DCMAKE_CXX_COMPILER=g++
        -DCMAKE_C_COMPILER=gcc
        -DCMAKE_BUILD_TYPE=Release
        -Dprotobuf_BUILD_TESTS=OFF
        -Dprotobuf_BUILD_SHARED_LIBS=OFF
        -DBUILD_SHARED_LIBS=OFF
        -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto
        -DCMAKE_CXX_STANDARD=20
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
        -DABSL_OPTION_USE_STD_ORDERING=0
        -DCMAKE_CXX_FLAGS='-static\ -I${PROJECT_SOURCE_DIR}/cmake_more_cache/absl/include'
        -DCMAKE_C_FLAGS='-static\ -I${PROJECT_SOURCE_DIR}/cmake_more_cache/absl/include'
        -Dprotobuf_WITH_ZLIB=OFF
        -Dprotobuf_ABSL_PROVIDER=package
        -DCMAKE_PREFIX_PATH=${PROJECT_SOURCE_DIR}/cmake_more_cache/absl/lib/cmake/absl
        -DABSL_ROOT_DIR=${PROJECT_SOURCE_DIR}/cmake_more_cache/absl/lib/cmake/absl
        -B ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto_build
    BUILD_COMMAND ninja
    INSTALL_COMMAND ninja install
)


ExternalProject_Add(gnsEX
    SOURCE_DIR ${submodules_path}/gns
    CMAKE_ARGS
        ${GLOBAL_DEFAULT_ARGS}
        -G Ninja
        -DCMAKE_CXX_COMPILER=g++
        -DCMAKE_C_COMPILER=gcc
        -DCMAKE_BUILD_TYPE=Release
        -DProtobuf_USE_STATIC_LIBS=ON
        -Dprotobuf_BUILD_TESTS=OFF
        -Dprotobuf_BUILD_SHARED_LIBS=OFF
        -DCMAKE_CXX_STANDARD=20
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
        -DOPENSSL_USE_STATIC_LIBS=ON
        -DProtobuf_LIBRARIES=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/lib
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
        "-DCMAKE_EXE_LINKER_FLAGS=$<$<BOOL:${WIN32}>:-static -lws2_32 -lcrypt32>" #testing
        #-DCMAKE_CXX_FLAGS='-D\ STEAMNETWORKINGSOCKETS_STATIC_LINK=1\ --static\ -static\ -I${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/include\ -I${PROJECT_SOURCE_DIR}/cmake_more_cache/absl/include\ -L${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/lib\ -L${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/absl\ -lprotobuf\ -lprotoc\ -lutf8_range\ -labsl_synchronization\ -labsl_symbolize\ -labsl_strings\ -labsl_str_format_internal\ -labsl_strings_internal\ -labsl_log_initialize\ -labsl_log_entry\ -labsl_log_flags\ -labsl_log_severity\ -labsl_log_internal_check_op\ -labsl_log_internal_conditions\ -labsl_log_internal_message\ -labsl_log_internal_nullguard\ -labsl_log_internal_proto\ -labsl_log_internal_format\ -labsl_log_internal_globals\ -labsl_log_internal_log_sink_set\ -labsl_log_sink\ -labsl_raw_logging_internal\ -labsl_log_globals\ -labsl_cord\ -labsl_cordz_info\ -labsl_cordz_handle\ -labsl_cordz_functions\ -labsl_cord_internal\ -labsl_crc_cord_state\ -labsl_crc32c\ -labsl_crc_internal\ -labsl_exponential_biased\ -labsl_synchronization\ -labsl_time\ -labsl_time_zone\ -labsl_int128\ -labsl_examine_stack\ -labsl_stacktrace\ -labsl_symbolize\ -labsl_demangle_internal\ -labsl_debugging_internal\ -labsl_malloc_internal\ -labsl_throw_delegate\ -labsl_strerror\ -labsl_raw_hash_set\ -labsl_hash\ -labsl_city\ -labsl_low_level_hash\ -labsl_base\ -labsl_spinlock_wait\ -labsl_status\ -std=c++20\ -lstdc++'
        -DCMAKE_CXX_FLAGS='--static\ -static\ -I${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/include\ -I${PROJECT_SOURCE_DIR}/cmake_more_cache/absl/include\ -L${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/lib\ -L${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/absl\ -lprotobuf-lite\ -lprotobuf\ -lprotoc\ -lutf8_range\ -std=c++20\ -lstdc++\ -lpthread\ -lupb\ -lutf8_validity'
        #-DCMAKE_CXX_FLAGS='-static'
        -B "${PROJECT_SOURCE_DIR}/cmake_more_cache/gns_build"
    BUILD_COMMAND ninja
    INSTALL_COMMAND ninja install
    DEPENDS protoEX
)




set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "set" FORCE)


set(BUILD_SHARED_LIBS OFF CACHE BOOL "set" FORCE)


# will have to add the rest later..
set(FASTGLTF_INSTALL OFF CACHE BOOL "no install for fastgltf" FORCE)
set(VkBootstrap_INSTALL OFF CACHE BOOL "no vkb install either" FORCE)


add_subdirectory(${submodules_path}/simdjson)
add_subdirectory(${submodules_path}/fastgltf)
add_subdirectory(${submodules_path}/sdl)
add_subdirectory(${submodules_path}/sdlimg)
add_subdirectory(${submodules_path}/glm)
add_subdirectory(${submodules_path}/vk_boot)
# rip
target_compile_options(vk-bootstrap PRIVATE -mno-avx512f -mno-avx2 -mno-avx)
add_subdirectory(${submodules_path}/vma)


file(GLOB PROJECT_HEADERS "include/*.hpp")
file(GLOB PROJECT_SOURCES "src/main.cpp" "src/*/*.cpp")
set(OTHER_SOURCES
    ${submodules_path}/imgui/imgui.cpp
    ${submodules_path}/imgui/backends/imgui_impl_vulkan.cpp
    ${submodules_path}/imgui/backends/imgui_impl_sdl3.cpp
    ${submodules_path}/imgui/misc/cpp/imgui_stdlib.cpp
    ${submodules_path}/imgui/imgui_tables.cpp
    ${submodules_path}/imgui/imgui_widgets.cpp
    ${submodules_path}/imgui/imgui_draw.cpp
    ${submodules_path}/imgui/imgui_demo.cpp
)

add_executable(rview2 ${OTHER_SOURCES} ${PROJECT_SOURCES} ${PROJECT_HEADERS})

#worst decision so far
target_sources(rview2
    PUBLIC
        FILE_SET CXX_MODULES 
        FILES
            mods/tex.cxxm
)

set(SLANG_COMMON_FLAGS
    -target spirv 
    -capability SPIRV_1_6
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWDbg>>:-g>
    $<$<OR:$<CONFIG:Release>,$<CONFIG:Distribution>,$<CONFIG:BleedingEdge>>:-O3>
)


# consider only statically -static-libgcc -static-libstdc++ idk
target_link_options(rview2 PRIVATE
    "$<$<OR:$<CONFIG:Release>,$<CONFIG:Distribution>,$<CONFIG:BleedingEdge>>:-static>"
    # no idea here..
    # "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWDbg>,$<CONFIG:Sanitize>>:-static>"
    "$<$<AND:$<PLATFORM_ID:Windows>,$<OR:$<CONFIG:Distribution>,$<CONFIG:Release>,$<CONFIG:BleedingEdge>>>:-mwindows>"
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
add_shader(SOURCE gen.slang ENTRY vxmain STAGE vertex OUTPUT_NAME vx.spv)
add_shader(SOURCE gen.slang ENTRY pxmain STAGE pixel OUTPUT_NAME px.spv)
# add_shader(SOURCE gen.slang ENTRY cxmain STAGE compute OUTPUT_NAME cx.spv)
add_shader(SOURCE particle.slang ENTRY vxmain STAGE vertex OUTPUT_NAME pvx.spv)
add_shader(SOURCE particle.slang ENTRY pxmain STAGE pixel OUTPUT_NAME ppx.spv)
add_shader(SOURCE particle.slang ENTRY cxmain STAGE compute OUTPUT_NAME pcx.spv)
add_shader(SOURCE red.slang ENTRY vxmain STAGE vertex OUTPUT_NAME rvx.spv)
add_shader(SOURCE red.slang ENTRY pxmain STAGE pixel OUTPUT_NAME rpx.spv)
add_shader(SOURCE red.slang ENTRY cxmain STAGE compute OUTPUT_NAME rcx.spv)

add_custom_target(shaders_tar ALL DEPENDS ${ALL_SHADER_SPV_FILES})
add_dependencies(rview2 shaders_tar)


file(GLOB_RECURSE RESOURCE_FILES "${PROJECT_SOURCE_DIR}/resources/*")
add_custom_command(
    OUTPUT "${PROJECT_BINARY_DIR}/resources.stamp"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/resources" "${PROJECT_BINARY_DIR}/resources"
    COMMAND ${CMAKE_COMMAND} -E touch "${PROJECT_BINARY_DIR}/resources.stamp"
    DEPENDS ${RESOURCE_FILES}
)
add_custom_target(resources_tar ALL DEPENDS "${PROJECT_BINARY_DIR}/resources.stamp")
add_dependencies(rview2 resources_tar)



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
        SDL3-static
        SDL3_image-static
        fastgltf
        glm
        Vulkan::Vulkan
        OpenSSL::SSL
        OpenSSL::Crypto
        vk-bootstrap::vk-bootstrap
        GameNetworkingSockets_s
        protobuf
        protoc
        upb
        protobuf-lite
        utf8_range
        utf8_validity
        OpenSSL::SSL
        OpenSSL::Crypto
        protobuf
        # absl_bad_any_cast_impl
        # absl_bad_optional_access
        # absl_bad_variant_access
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

target_include_directories(rview2
    BEFORE
    PUBLIC
        "${PROJECT_SOURCE_DIR}/cmake_more_cache/gns/include/GameNetworkingSockets"
        "${submodules_path}/stb"
        "${submodules_path}/imgui"
        "${submodules_path}/vma/include"
        "${submodules_path}/sdl/include"
        "${submodules_path}/jolt"
        "${submodules_path}/meshoptimizer/src"
        "${PROJECT_SOURCE_DIR}/include"
)



set_property(TARGET rview2 APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${PROJECT_BINARY_DIR}/resources)
set_property(TARGET rview2 APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${PROJECT_BINARY_DIR}/shaders)

install(TARGETS rview2 RUNTIME DESTINATION .)
install(FILES ${ALL_SHADER_SPV_FILES} DESTINATION shaders)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/" DESTINATION resources)

