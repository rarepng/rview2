cmake_minimum_required(VERSION 4.0)
project(rview2 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)

find_package(Vulkan REQUIRED)

set(OPENSSL_USE_STATIC_LIBS ON)
set(ZLIB_USE_STATIC_LIBS ON)
find_package(OpenSSL REQUIRED)

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

# brute force &#@! it we ball
string(APPEND CMAKE_SHARED_LINKER_FLAGS " -static")
string(APPEND CMAKE_EXE_LINKER_FLAGS " -static")

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

set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "set" FORCE)
# will be removed.. eventually (hopefully)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "set" FORCE)


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
    ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp
)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_BUILD_TYPE STREQUAL "x") # scrapped

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

include(CMakeLocal.txt OPTIONAL)

target_compile_definitions(rview2 PRIVATE
"RVIEW_DEBUG=$<IF:$<CONFIG:Debug>,1,0>"
)

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
        "-Wl,--end-group"
        "$<$<PLATFORM_ID:Windows>:d3d12>"
        "$<$<PLATFORM_ID:Windows>:d3d11>"
        "$<$<PLATFORM_ID:Windows>:dxgi>"
        "$<$<PLATFORM_ID:Windows>:spout_dx>"
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
        pthread                               #windows
        # ws2_32                              #windows
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
        "${imguizmo_SOURCE_DIR}/src"
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
target_link_options(rview2 PRIVATE
    # "$<$<OR:$<CONFIG:Release>,$<CONFIG:Distribution>,$<CONFIG:BleedingEdge>,$<CONFIG:MinSizeRel>>:-static>"
    # "$<$<OR:$<CONFIG:Release>,$<CONFIG:Distribution>,$<CONFIG:BleedingEdge>,$<CONFIG:MinSizeRel>>:-static-libstdc++>"
    # "$<$<OR:$<CONFIG:Release>,$<CONFIG:Distribution>,$<CONFIG:BleedingEdge>,$<CONFIG:MinSizeRel>>:-static-libgcc>"
    # no idea here..
    # "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>,$<CONFIG:sanitize>>:-static>"

    "$<$<AND:$<PLATFORM_ID:Windows>,$<OR:$<CONFIG:Distribution>,$<CONFIG:Release>,$<CONFIG:BleedingEdge>,$<CONFIG:MinSizeRel>>>:-mwindows>"
    # "-static"
    "-static-libgcc" "-static-libstdc++"
)



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

add_custom_target(shaders_tar ALL DEPENDS ${ALL_SHADER_SPV_FILES})
add_dependencies(rview2 shaders_tar)


add_custom_target(assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_SOURCE_DIR}/boot.json"
        "$<TARGET_FILE_DIR:rview2>/boot.json"
        
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        "${PROJECT_SOURCE_DIR}/resources"
        "$<TARGET_FILE_DIR:rview2>/resources"
        
    COMMENT "syncing resources and configs..."
)

add_dependencies(rview2 assets)

add_custom_target(state
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE_DIR:rview2>/state.json"
        "${PROJECT_SOURCE_DIR}/state.json"
        
    COMMENT "reverse syncing state.json"
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

