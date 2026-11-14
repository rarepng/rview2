cmake_minimum_required(VERSION 4.0)

if(ANDROID)
    set(CMAKE_ANDROID_EXCEPTIONS ON)
    set(CMAKE_ANDROID_RTTI ON)
    
    set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES "")
endif()

project(rview2 LANGUAGES CXX C)

if(ANDROID)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)

if(ANDROID)# yea *$&@ u ndk
    string(REPLACE "-std=c++11" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "-fno-exceptions" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    string(REPLACE "-fno-rtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

if(ANDROID AND DEFINED CUSTOM_LIBCXX_DIR)
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:-fexceptions>"
        "$<$<COMPILE_LANGUAGE:CXX>:-frtti>"
    )
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:-nostdinc++>"
        "$<$<COMPILE_LANGUAGE:CXX>:-nostdlib++>"
        "$<$<COMPILE_LANGUAGE:CXX>:-isystem${CUSTOM_LIBCXX_DIR}/include/aarch64-linux-android/c++/v1>"
        "$<$<COMPILE_LANGUAGE:CXX>:-isystem${CUSTOM_LIBCXX_DIR}/include/c++/v1>"
        
        "$<$<COMPILE_LANGUAGE:CXX>:-include${CMAKE_SOURCE_DIR}/external/android_hacks.hpp>"
    )
endif()

if(ANDROID)
set(HAVE_WL_VERSION_SCRIPT TRUE CACHE INTERNAL "Bypass SDL linker check cause it's $&*@ing broken")
endif()

find_package(Vulkan REQUIRED)

set(OPENSSL_USE_STATIC_LIBS ON)
set(ZLIB_USE_STATIC_LIBS ON)
if(NOT ANDROID)
find_package(OpenSSL REQUIRED)
endif()

if(ANDROID AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" "-print-resource-dir"
        OUTPUT_VARIABLE CLANG_RESOURCE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
    )
    file(TO_CMAKE_PATH "${CLANG_RESOURCE_DIR}" CLANG_RESOURCE_DIR)
endif()


set(CPM_PATH "${CMAKE_BINARY_DIR}/_deps/CPM.cmake")
if(EXISTS "${CPM_PATH}")
    file(SIZE "${CPM_PATH}" _cpm_size)
    if(_cpm_size EQUAL 0)
        file(REMOVE "${CPM_PATH}")
    endif()
endif()
if(NOT EXISTS "${CPM_PATH}")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake
        "${CPM_PATH}"
        STATUS status
    )
    list(GET status 0 code)
    if(NOT code EQUAL 0)
        file(REMOVE "${CPM_PATH}")
        message(FATAL_ERROR "CPM download failed. Cleaned up empty file.")
    endif()
endif()
include(${CPM_PATH})
if(NOT COMMAND CPMAddPackage)
    message(FATAL_ERROR "CPM broke again :(")
endif()

get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(is_multi_config)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo;Distribution;BleedingEdge;sanitize;clanged;mermaid;MinSizeRel" CACHE STRING "build types & other ghosts" FORCE)
endif()

set(CMAKE_MAP_IMPORTED_CONFIG_BLEEDINGEDGE Release RelWithDebInfo MinSizeRel Debug "")
set(CMAKE_MAP_IMPORTED_CONFIG_DISTRIBUTION Release RelWithDebInfo MinSizeRel Debug "")
set(CMAKE_MAP_IMPORTED_CONFIG_SANITIZE Debug RelWithDebInfo Release "")
set(CMAKE_MAP_IMPORTED_CONFIG_CLANGED Debug Release "")
set(CMAKE_MAP_IMPORTED_CONFIG_MERMAID Debug Release "")

# brute force &#@! it we ball
if(NOT ANDROID)
string(APPEND CMAKE_SHARED_LINKER_FLAGS " -static")
string(APPEND CMAKE_EXE_LINKER_FLAGS " -static")
endif()

set(submodules_path
        ${PROJECT_SOURCE_DIR}/sub
)

if(NOT ANDROID)
find_program(ASTYLE_EXECUTABLE astyle)
if(ASTYLE_EXECUTABLE)
    file(GLOB_RECURSE ALL_SOURCE_FILES 
        "src/*.cpp"
        "include/*.hpp"
        "mods/*.cxxm"
    )
    add_custom_target(astyleformat
        COMMAND ${ASTYLE_EXECUTABLE} --options=${CMAKE_SOURCE_DIR}/.astylerc ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "astyling..."
    )
endif()
endif()
find_program(CLANG_FORMAT_EXECUTABLE clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    file(GLOB_RECURSE ALL_SOURCE_FILES 
        "src/*.cpp"
        "include/*.hpp"
        "mods/*.cxxm"
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i -style=file ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "clang-formating..."
    )
endif()

if(NOT ANDROID)
set(CMAKE_SKIP_INSTALL_RULES ON)
# ----------------------------------------------------------------------#
# Polyfill for legacy protobuf_generate_cpp curtousy of LLMs be careful #
# ----------------------------------------------------------------------#
function(protobuf_generate_cpp SRCS HDRS)
    set(out_srcs "")
    set(out_hdrs "")
    
    foreach(proto_file ${ARGN})
        get_filename_component(abs_file ${proto_file} ABSOLUTE)
        get_filename_component(basename ${proto_file} NAME_WE)
        
        get_filename_component(proto_dir ${abs_file} DIRECTORY)
        
        set(src_out "${CMAKE_CURRENT_BINARY_DIR}/${basename}.pb.cc")
        set(hdr_out "${CMAKE_CURRENT_BINARY_DIR}/${basename}.pb.h")
        
        list(APPEND out_srcs "${src_out}")
        list(APPEND out_hdrs "${hdr_out}")

        add_custom_command(
            OUTPUT "${src_out}" "${hdr_out}"
            COMMAND protobuf::protoc
            ARGS --cpp_out "${CMAKE_CURRENT_BINARY_DIR}" 
                 -I "${proto_dir}"
                 -I "${CMAKE_CURRENT_SOURCE_DIR}"
                 -I "${CMAKE_SOURCE_DIR}/sub/protobuf/src"
                 "${abs_file}"
            DEPENDS "${abs_file}" protobuf::protoc
            COMMENT "Compiling protocol buffer ${basename}"
            VERBATIM
        )
    endforeach()
    
    set(${SRCS} "${out_srcs}" PARENT_SCOPE)
    set(${HDRS} "${out_hdrs}" PARENT_SCOPE)
endfunction()

CPMAddPackage(
    NAME absl
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/sub/absl
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "ABSL_PROPAGATE_CXX_STD ON"
        "BUILD_TESTING OFF"
        "ABSL_ENABLE_INSTALL OFF"
)

CPMAddPackage(
    NAME protobuf
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/sub/protobuf
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "protobuf_BUILD_TESTS OFF"
        "protobuf_BUILD_SHARED_LIBS OFF"
        "protobuf_BUILD_EXAMPLES OFF"
        "protobuf_WITH_ZLIB OFF"
        "protobuf_ABSL_PROVIDER package"
        "protobuf_INSTALL OFF"
)

CPMAddPackage(
    NAME GameNetworkingSockets
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/sub/gns
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "GameNetworkingSockets_BUILD_SHARED OFF"
        "GameNetworkingSockets_BUILD_TESTS OFF"
        "GameNetworkingSockets_BUILD_EXAMPLES OFF"
        "Protobuf_USE_STATIC_LIBS ON"
)
if(TARGET GameNetworkingSockets_s)
    target_compile_definitions(GameNetworkingSockets_s PRIVATE HAVE_EVP_MD_CTX_FREE=1)
    get_target_property(_gns_links GameNetworkingSockets_s LINK_LIBRARIES)
    if(_gns_links)
        list(REMOVE_ITEM _gns_links "protobuf" "-lprotobuf")
        set_target_properties(GameNetworkingSockets_s PROPERTIES LINK_LIBRARIES "${_gns_links}")
    endif()

    get_target_property(_gns_interface GameNetworkingSockets_s INTERFACE_LINK_LIBRARIES)
    if(_gns_interface)
        list(REMOVE_ITEM _gns_interface "protobuf" "-lprotobuf")
        set_target_properties(GameNetworkingSockets_s PROPERTIES INTERFACE_LINK_LIBRARIES "${_gns_interface}")
    endif()
    target_link_libraries(GameNetworkingSockets_s PRIVATE 
        OpenSSL::SSL 
        OpenSSL::Crypto
        protobuf::libprotobuf
        utf8_range
        utf8_validity
    )
endif()
set_property(DIRECTORY ${PROJECT_SOURCE_DIR}/sub/absl APPEND PROPERTY COMPILE_DEFINITIONS ABSL_OPTION_USE_STD_ORDERING=0 ABSL_USES_STD_ORDERING=0)
set_property(DIRECTORY ${PROJECT_SOURCE_DIR}/sub/protobuf APPEND PROPERTY COMPILE_DEFINITIONS ABSL_OPTION_USE_STD_ORDERING=0 ABSL_USES_STD_ORDERING=0)
set(CMAKE_SKIP_INSTALL_RULES OFF)
endif()

set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "set" FORCE)
if(NOT ANDROID)
# will be removed.. eventually (hopefully)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "set" FORCE)
endif()


set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)

CPMAddPackage(
    NAME glm
    GITHUB_REPOSITORY g-truc/glm
    GIT_TAG 1.0.1
    DOWNLOAD_ONLY YES
    EXCLUDE_FROM_ALL YES
)
add_library(glm INTERFACE)
target_include_directories(glm SYSTEM INTERFACE ${glm_SOURCE_DIR})

if(ANDROID)
set(CMAKE_HAVE_LIBC_PTHREAD TRUE CACHE BOOL "Bypass thread check (built into Android Bionic)")
endif()
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
if(ANDROID)
set(DUMMY_SDL3_DIR "${CMAKE_CURRENT_BINARY_DIR}/dummy_sdl3")
file(MAKE_DIRECTORY "${DUMMY_SDL3_DIR}")
file(WRITE "${DUMMY_SDL3_DIR}/SDL3Config.cmake" "set(SDL3_FOUND TRUE)\n")
file(WRITE "${DUMMY_SDL3_DIR}/SDL3ConfigVersion.cmake" 
    "set(PACKAGE_VERSION \"3.4.4\")\nset(PACKAGE_VERSION_COMPATIBLE TRUE)\n"
)
set(SDL3_DIR "${DUMMY_SDL3_DIR}")
endif()
CPMAddPackage(
    NAME sdlimg
    GITHUB_REPOSITORY libsdl-org/SDL_image
    GIT_TAG release-3.4.4
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "SDL_SHARED OFF"
        "SDL_STATIC ON"
        "SDL_TESTS OFF"
        "BUILD_SHARED_LIBS OFF"
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
    NAME imguizmo
    GITHUB_REPOSITORY cedricguillemet/imguizmo
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
    NAME Spout2
    GITHUB_REPOSITORY leadedge/Spout2
    GIT_TAG master
    EXCLUDE_FROM_ALL YES
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME rpmalloc
    GITHUB_REPOSITORY rampantpixels/rpmalloc
    GIT_TAG 1.4.5
    EXCLUDE_FROM_ALL YES
    DOWNLOAD_ONLY YES
)

if(rpmalloc_ADDED)
    add_library(rpmalloc_static STATIC ${rpmalloc_SOURCE_DIR}/rpmalloc/rpmalloc.c)
    target_include_directories(rpmalloc_static PUBLIC ${rpmalloc_SOURCE_DIR}/rpmalloc)
    
    target_compile_definitions(rpmalloc_static PUBLIC 
        ENABLE_STATISTICS=0 
        ENABLE_ASSERTS=0
    )
    
    target_compile_definitions(rpmalloc_static PRIVATE
        "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:Distribution>,$<CONFIG:BleedingEdge>,$<CONFIG:MinSizeRel>>:NDEBUG>"
    )
    
    target_compile_options(rpmalloc_static PRIVATE
        "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:Distribution>,$<CONFIG:BleedingEdge>,$<CONFIG:MinSizeRel>>:-O3>"
        "$<$<CONFIG:BleedingEdge>:-march=native>"
        "$<$<CONFIG:Distribution>:-march=x86-64-v3>"
    )
endif()

if(ANDROID)
    if(TARGET rpmalloc_static)
        target_compile_definitions(rpmalloc_static PRIVATE ENABLE_INITIAL_EXEC=0)
        target_compile_options(rpmalloc_static PRIVATE -femulated-tls)
    endif()
    if(TARGET TracyClient)
        target_compile_options(TracyClient PRIVATE -femulated-tls)
    endif()
endif()

if(WIN32)
    add_library(spout_dx STATIC EXCLUDE_FROM_ALL
        ${Spout2_SOURCE_DIR}/SPOUTSDK/SpoutDirectX/SpoutDX/SpoutDX.cpp
        ${Spout2_SOURCE_DIR}/SPOUTSDK/SpoutGL/SpoutDirectX.cpp
        ${Spout2_SOURCE_DIR}/SPOUTSDK/SpoutGL/SpoutSenderNames.cpp
        ${Spout2_SOURCE_DIR}/SPOUTSDK/SpoutGL/SpoutSharedMemory.cpp
        ${Spout2_SOURCE_DIR}/SPOUTSDK/SpoutGL/SpoutUtils.cpp
        ${Spout2_SOURCE_DIR}/SPOUTSDK/SpoutGL/SpoutFrameCount.cpp
        ${Spout2_SOURCE_DIR}/SPOUTSDK/SpoutGL/SpoutCopy.cpp
    )
    
    target_compile_options(spout_dx PRIVATE -mssse3) # idk about this one..
    target_include_directories(spout_dx PUBLIC 
        ${Spout2_SOURCE_DIR}/SPOUTSDK/SpoutDirectX/SpoutDX
        ${Spout2_SOURCE_DIR}/SPOUTSDK/SpoutGL
    )
    
    target_link_libraries(spout_dx PUBLIC d3d12 d3d11 dxgi)
endif()

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
if(NOT ANDROID)
target_compile_options(vk-bootstrap PRIVATE -mno-avx512f -mno-avx2 -mno-avx)
target_compile_definitions(simdjson PUBLIC SIMDJSON_IMPLEMENTATION_ICELAKE=0 SIMDJSON_IMPLEMENTATION_HASWELL=0)
endif()


file(GLOB_RECURSE PROJECT_HEADERS "include/*.hpp")
file(GLOB_RECURSE PROJECT_SOURCES "src/*.cpp")
if(ANDROID)
    list(REMOVE_ITEM PROJECT_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/include/net/netbuffer.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/net/netclient.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/net/netserver.hpp"
    )
    list(REMOVE_ITEM PROJECT_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/net/netclient.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/net/netserver.cpp"
    )
endif()

set(OTHER_SOURCES
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp
)

if(ANDROID)
set_source_files_properties(${PROJECT_SOURCES} PROPERTIES LANGUAGE CXX)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _arch)
if(_arch MATCHES "(x86_64|amd64|i386|i686)")
    set(RVIEW_RELAX_HEADER "#include <immintrin.h>")
    set(RVIEW_RELAX_INSTRUCTION "_mm_pause()")
elseif(_arch MATCHES "(arm|aarch64)")
    set(RVIEW_RELAX_HEADER "")
    set(RVIEW_RELAX_INSTRUCTION "asm volatile(\"yield\" ::: \"memory\")")
else()
    set(RVIEW_RELAX_HEADER "#include <thread>")
    set(RVIEW_RELAX_INSTRUCTION "std::this_thread::yield()")
endif()

configure_file(
    "${PROJECT_SOURCE_DIR}/include/dbg/cpu_relax.hpp.in"
    "${PROJECT_BINARY_DIR}/generated/include/cpu_relax.hpp"
    @ONLY
)


if(ANDROID)
add_library(rview2 SHARED ${OTHER_SOURCES} ${PROJECT_SOURCES} ${PROJECT_HEADERS})
else()
add_executable(rview2 ${OTHER_SOURCES} ${PROJECT_SOURCES} ${PROJECT_HEADERS})
target_compile_definitions(rview2 PRIVATE SDL_MAIN_HANDLED)
endif()

include(CMakeLocal.txt OPTIONAL)

target_compile_definitions(rview2 PRIVATE
"RVIEW_DEBUG=$<IF:$<CONFIG:Debug>,1,0>"
"RVIEW_DEMO=$<BOOL:${RVIEW_DEMO}>"

)

if(NOT ANDROID)
target_link_directories(rview2 PRIVATE
    ${PROJECT_SOURCE_DIR}/cmake_more_cache/gns/lib
    ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/lib
    ${PROJECT_SOURCE_DIR}/cmake_more_cache/absl/lib
    #${OPENSSL_ROOT}/lib64
)
target_compile_definitions(rview2 PRIVATE
    STEAMNETWORKINGSOCKETS_STATIC_LINK=1
    ABSL_OPTION_USE_STD_ORDERING=0
    ABSL_USES_STD_ORDERING=0
)
endif()

target_compile_definitions(rview2 PRIVATE
    RVIEW_HEADLESS=0
)

if(ANDROID)
    if(NOT DEFINED CUSTOM_LIBCXX_DIR)
        set(CUSTOM_LIBCXX_DIR "C:/miasma/androidlibcxx")
    endif()

    set(CUSTOM_LIBCXX_LIB_DIR "${CUSTOM_LIBCXX_DIR}/lib")
    if(EXISTS "${CUSTOM_LIBCXX_DIR}/lib/aarch64-linux-android")
        set(CUSTOM_LIBCXX_LIB_DIR "${CUSTOM_LIBCXX_DIR}/lib/aarch64-linux-android")
    elseif(EXISTS "${CUSTOM_LIBCXX_DIR}/lib/aarch64-unknown-linux-android")
        set(CUSTOM_LIBCXX_LIB_DIR "${CUSTOM_LIBCXX_DIR}/lib/aarch64-unknown-linux-android")
    endif()

    target_link_directories(rview2 PRIVATE "${CUSTOM_LIBCXX_LIB_DIR}")
    
endif()

target_link_libraries(rview2
    PRIVATE
        "$<$<CONFIG:Debug>:TracyClient>"
        SDL3::SDL3-static
        SDL3_image::SDL3_image-static
        fastgltf
        glm
        Vulkan::Vulkan
        vk-bootstrap::vk-bootstrap
        rpmalloc_static
)
if(NOT ANDROID)
target_link_libraries(rview2
    PRIVATE
        "-Wl,--start-group"
        GameNetworkingSockets_s
        OpenSSL::SSL
        OpenSSL::Crypto
        "-Wl,--end-group"
)
endif()
target_link_libraries(rview2
    PRIVATE
        "$<$<PLATFORM_ID:Windows>:d3d12>"
        "$<$<PLATFORM_ID:Windows>:d3d11>"
        "$<$<PLATFORM_ID:Windows>:dxgi>"
        "$<$<PLATFORM_ID:Windows>:spout_dx>"
)
if(NOT ANDROID)
if(UNIX)
target_link_libraries(rview2
    PRIVATE
        c
        pthread
)
else()
target_link_libraries(rview2
    PRIVATE
        pthread                               #windows
        # ws2_32                              #windows
        DbgHelp                               #windows
        winmm                                 #windows
        iphlpapi                              #windows
)
endif()
endif()

if(ANDROID)
    set(RVIEW2_PLATFORM_INCLUDE_DIR "${sdl_SOURCE_DIR}/include")
else()
    set(RVIEW2_PLATFORM_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/cmake_more_cache/gns/include/GameNetworkingSockets")
endif()

target_include_directories(rview2
    BEFORE
    PUBLIC
        "${RVIEW2_PLATFORM_INCLUDE_DIR}"
        "${stb_SOURCE_DIR}"
        "${imgui_SOURCE_DIR}"
        "${imguizmo_SOURCE_DIR}/src"
        "${simdjson_SOURCE_DIR}/include"
        "${tracy_SOURCE_DIR}/public"
        "${submodules_path}/vma/include"
        "${submodules_path}/jolt"
        "${submodules_path}/meshoptimizer/src"
        "${PROJECT_BINARY_DIR}/generated/include" # in bin just in case of cross compiling
        "${PROJECT_SOURCE_DIR}/include"
)


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
if(NOT ANDROID)
target_link_options(rview2 PRIVATE
    # no idea here..
    # "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>,$<CONFIG:sanitize>>:-static>"

    "$<$<AND:$<PLATFORM_ID:Windows>,$<OR:$<CONFIG:Distribution>,$<CONFIG:Release>,$<CONFIG:BleedingEdge>,$<CONFIG:MinSizeRel>>>:-mwindows>"
    # "-static"
    "-static-libgcc" "-static-libstdc++"
)
endif()

target_link_libraries(rview2 PRIVATE
    # $<$<BOOL:${UNIX}>:c;pthread>
    # $<$<BOOL:${WIN32}>:pthread;DbgHelp;winmm;iphlpapi>
    "$<$<BOOL:${ANDROID}>:c++>"
    "$<$<BOOL:${ANDROID}>:c++abi>"
)

# add_custom_command(TARGET rview2 POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#     "$<TARGET_FILE_DIR:rview2>"
# )
if(ANDROID)
    set(ANDROID_APP_DIR "${CMAKE_SOURCE_DIR}/androidbuild/app/src/main")
    set(ASSET_OUT_DIR "${ANDROID_APP_DIR}/assets")
    set(JNI_OUT_DIR "${ANDROID_APP_DIR}/jniLibs/${CMAKE_ANDROID_ARCH_ABI}")
else()
    set(ASSET_OUT_DIR "$<TARGET_FILE_DIR:rview2>")
endif()

if(ANDROID)
    add_custom_command(TARGET rview2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${JNI_OUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:rview2>"
            "${JNI_OUT_DIR}/librview2.so"
        COMMENT "Copying compiled librview2.so -> ${JNI_OUT_DIR}"
    )
endif()



set(SLANG_COMMON_FLAGS
    -target spirv 
    -capability SPIRV_1_6
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:-g>
    $<$<OR:$<CONFIG:Release>,$<CONFIG:Distribution>,$<CONFIG:BleedingEdge>,$<CONFIG:MinSizeRel>>:-O3>
)


# wip wip
# manually adding shaders for now
function(add_shader)
    set(options "")
    set(oneValueArgs SOURCE ENTRY STAGE OUTPUT_NAME)
    set(multiValueArgs "")
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(SHADER_SOURCE "${PROJECT_SOURCE_DIR}/shaders/${ARG_SOURCE}")
    set(SPV_OUTPUT "${PROJECT_BINARY_DIR}/$<CONFIG>/shaders/${ARG_OUTPUT_NAME}")
    get_filename_component(OUT_DIR ${SPV_OUTPUT} DIRECTORY)
    add_custom_command(
        OUTPUT "${SPV_OUTPUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OUT_DIR}"
        COMMAND slangc
            -entry ${ARG_ENTRY}
            -stage ${ARG_STAGE}
            -profile sm_6_10 # cba
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

add_custom_target(shaders_tar ALL DEPENDS ${ALL_SHADER_SPV_FILES})


if(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(ASSET_OUT_DIR "${CMAKE_SOURCE_DIR}/androidbuild/app/src/main/assets")

add_custom_target(assets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSET_OUT_DIR}"
    
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/boot.json"
        "${ASSET_OUT_DIR}/boot.json"
        
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${PROJECT_SOURCE_DIR}/resources"
        "${ASSET_OUT_DIR}/resources"

    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${PROJECT_BINARY_DIR}/$<CONFIG>/shaders" 
        "${ASSET_OUT_DIR}/shaders"
        
    COMMENT "Syncing resources and configs..."
)
else()
    set(ASSET_OUT_DIR "$<TARGET_FILE_DIR:rview2>")

add_custom_target(assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/boot.json"
        "$<TARGET_FILE_DIR:rview2>/boot.json"
        
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${PROJECT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:rview2>/resources"
    
    COMMENT "syncing resources, shaders and configs..."
)
endif()
add_dependencies(assets shaders_tar) # full circle
add_dependencies(rview2 assets)

add_custom_target(state
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE_DIR:rview2>/state.json"
        "${PROJECT_SOURCE_DIR}/state.json"
        
    COMMENT "reverse syncing state.json"
)


set_property(TARGET rview2 APPEND PROPERTY ADDITIONAL_CLEAN_FILES "$<TARGET_FILE_DIR:rview2>/resources")
set_property(TARGET rview2 APPEND PROPERTY ADDITIONAL_CLEAN_FILES "$<TARGET_FILE_DIR:rview2>/shaders")
set_property(TARGET rview2 APPEND PROPERTY ADDITIONAL_CLEAN_FILES "$<TARGET_FILE_DIR:rview2>/boot.json")
if(NOT ANDROID)
install(TARGETS rview2 RUNTIME DESTINATION $<CONFIG> CONFIGURATIONS Release Distribution BleedingEdge MinSizeRel)
install(FILES ${ALL_SHADER_SPV_FILES} DESTINATION $<CONFIG>/shaders)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/resources/" DESTINATION $<CONFIG>/resources)
install(FILES "${PROJECT_SOURCE_DIR}/boot.json" DESTINATION $<CONFIG>)
endif()
if(ANDROID)
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES 
        "${CMAKE_SOURCE_DIR}/dist"
        "${CMAKE_SOURCE_DIR}/androidbuild/app/build"
        "${CMAKE_SOURCE_DIR}/androidbuild/.gradle"
        "${CMAKE_SOURCE_DIR}/androidbuild/app/src/main/assets"
        "${CMAKE_SOURCE_DIR}/androidbuild/app/src/main/jniLibs"
    )
endif()

if(ANDROID)
    if(CMAKE_HOST_WIN32)
        set(GRADLE_EXE "${CMAKE_SOURCE_DIR}/androidbuild/gradlew.bat")
    else()
        set(GRADLE_EXE "${CMAKE_SOURCE_DIR}/androidbuild/gradlew")
    endif()

    add_custom_target(apk
        COMMAND "${GRADLE_EXE}" assemble$<IF:$<CONFIG:Debug>,Debug,Release>
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/androidbuild"
        DEPENDS rview2 assets
        COMMENT ">>> Packaging APK via Gradle..."
        USES_TERMINAL
    )
    install(FILES "${CMAKE_SOURCE_DIR}/androidbuild/app/build/outputs/apk/$<IF:$<CONFIG:Debug>,debug,release>/app-$<IF:$<CONFIG:Debug>,debug,release>.apk" 
            DESTINATION "${CMAKE_SOURCE_DIR}/dist"
            RENAME "rview2-$<IF:$<CONFIG:Debug>,debug,release>.apk"
    )
    add_custom_target(apk-clean
        COMMAND "${GRADLE_EXE}" clean
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/androidbuild"
        COMMENT ">>> Wiping Gradle cache..."
        USES_TERMINAL
    )
    set(ADB_FLAGS "")
    set(ADB_CONNECT_CMD "")
    if(DEFINED ADB_IP)
        set(ADB_TARGET "${ADB_IP}")
        if(DEFINED ADB_PORT)
            string(APPEND ADB_TARGET ":${ADB_PORT}")
        endif()
        set(ADB_FLAGS "-s" "${ADB_TARGET}")
        set(ADB_CONNECT_CMD COMMAND adb connect "${ADB_TARGET}")
    endif()
    add_custom_target(apk-install
        ${ADB_CONNECT_CMD}
        COMMAND adb ${ADB_FLAGS} install -r "${CMAKE_SOURCE_DIR}/androidbuild/app/build/outputs/apk/$<IF:$<CONFIG:Debug>,debug,release>/app-$<IF:$<CONFIG:Debug>,debug,release>.apk"
        DEPENDS apk
        COMMENT ">>> Pushing to device..."
        USES_TERMINAL
    )
    add_custom_target(apk-run
        ${ADB_CONNECT_CMD}
        COMMAND adb ${ADB_FLAGS} shell monkey -p "${APP_NAME}" -c android.intent.category.LAUNCHER 1
        # COMMAND adb ${ADB_FLAGS} shell am start -n "${APP_NAME}/org.libsdl.app.SDLActivity"
        DEPENDS apk-install
        COMMENT ">>> launching..."
        USES_TERMINAL
    )
endif()


if(WIN32)
    get_filename_component(RVIEW2_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

    install(CODE "
        if(POLICY CMP0207)
                cmake_policy(SET CMP0207 NEW)
        endif()
        message(STATUS \"resolving dynamic libraries for $<TARGET_FILE_NAME:rview2>...\")
        
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES \"\${CMAKE_INSTALL_PREFIX}/$<CONFIG>/$<TARGET_FILE_NAME:rview2>\"
            DIRECTORIES \"${RVIEW2_BIN_DIR}\"
            RESOLVED_DEPENDENCIES_VAR RESOLVED_DEPS
            UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED_DEPS
            PRE_EXCLUDE_REGEXES 
                \"api-ms-.*\"
                \"ext-ms-.*\"
            POST_EXCLUDE_REGEXES
                \".*[Ss]ystem32.*\"
                \".*[Ss]ys[Ww][Oo][Ww]64.*\"
                \".*[Ww]in[Ss]x[Ss].*\"
        )

        foreach(DEP \${RESOLVED_DEPS})
            get_filename_component(DEP_NAME \${DEP} NAME)
            string(TOLOWER \${DEP_NAME} LOWER_DEP_NAME)
            
            if(NOT LOWER_DEP_NAME MATCHES \"^(kernel32|kernelbase|user32|msvcrt|msvcp.*|advapi32|ws2_32|shell32|setupapi|ole32|oleaut32|gdi32|imm32|version|winmm|crypt32|dbghelp|iphlpapi|vulkan-1|ntdll|combase|sechost|rpcrt4|win32u|cfgmgr32|d3d11|d3d12|dxgi)\\\\.dll$\")
                message(STATUS \"deploying \${DEP_NAME}\")
                file(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}/$<CONFIG>\" TYPE SHARED_LIBRARY FILES \"\${DEP}\")
            endif()
        endforeach()
    "
    CONFIGURATIONS Release Distribution BleedingEdge MinSizeRel)
    # maybe ill setup a launcher pattern
endif()
