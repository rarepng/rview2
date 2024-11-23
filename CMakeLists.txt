cmake_minimum_required(VERSION 3.22)
project(raw LANGUAGES CXX)

include(ExternalProject)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Vulkan REQUIRED)
#find_package(libprotobuf)
#find_package(GameNetworkingSockets)

set(submodules_path
        ${PROJECT_SOURCE_DIR}/sub
)


#if(NOT IS_DIRECTORY ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto)
#execute_process(COMMAND pwsh "-c" "mkdir ${PROJECT_SOURCE_DIR}/cmake_more_cache")
#execute_process(COMMAND pwsh "-c" "mkdir ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto_build")
#execute_process(COMMAND pwsh "-c" "mkdir ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto")
#execute_process(COMMAND pwsh "-c" "mkdir ${PROJECT_SOURCE_DIR}/cmake_more_cache/gns")
#execute_process(COMMAND pwsh "-c" "cd ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto_build & cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_BUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto ${submodules_path}/protobuf/cmake")
#execute_process(COMMAND pwsh "-c" "cd ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto_build & ninja")
#execute_process(COMMAND pwsh "-c" "cd ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto_build & ninja install")
#execute_process(COMMAND pwsh "-c" "set PATH=%PATH%;${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/bin & cd ${PROJECT_SOURCE_DIR}/cmake_more_cache/gns_build & cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ${submodules_path}/gns")
#execute_process(COMMAND pwsh "-c" "set PATH=%PATH%;${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/bin & cd ${PROJECT_SOURCE_DIR}/cmake_more_cache/gns_build & ninja")
#execute_process(COMMAND pwsh "-c" "pwsh "-c" cd ${PROJECT_SOURCE_DIR}")
#endif()

#ExternalProject_Add()
#ExternalProject_Add(proto
#    SOURCE_DIR ${submodules_path}/protobuf/cmake
#    CMAKE_ARGS
#        #${GLOBAL_DEFAULT_ARGS}
#        -G Ninja
#        -DCMAKE_BUILD_TYPE=Release
#        -Dprotobuf_BUILD_TESTS=OFF
#        -Dprotobuf_BUILD_SHARED_LIBS=ON
#        -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto
#        -B ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto_build
#    BUILD_COMMAND pwsh "-c" "cd ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto_build & ninja & ninja install"
#)

#ExternalProject_Add(gns
#    SOURCE_DIR ${submodules_path}/gns
#    CMAKE_ARGS
#        #${GLOBAL_DEFAULT_ARGS}
#        -G Ninja
#        -DCMAKE_BUILD_TYPE=Release
#        -B ${PROJECT_SOURCE_DIR}/cmake_more_cache/gns
#    BUILD_COMMAND pwsh "-c" "ninja"
#)

add_subdirectory(${submodules_path}/fastgltf)
add_subdirectory(${submodules_path}/glfw)
add_subdirectory(${submodules_path}/glm)
#add_subdirectory(${submodules_path}/openssl)
#add_subdirectory(${submodules_path}/protobuf)
#add_subdirectory(${submodules_path}/gns)
add_subdirectory(${submodules_path}/simdjson)
add_subdirectory(${submodules_path}/vk_boot)
add_subdirectory(${submodules_path}/vma)

file(GLOB PROJECT_SOURCES "src/*.cpp")
set(OTHER_SOURCES
    ${submodules_path}/imgui/imgui.cpp
    ${submodules_path}/imgui/backends/imgui_impl_vulkan.cpp
    ${submodules_path}/imgui/backends/imgui_impl_glfw.cpp
    ${submodules_path}/imgui/misc/cpp/imgui_stdlib.cpp
    ${submodules_path}/imgui/imgui_tables.cpp
    ${submodules_path}/imgui/imgui_widgets.cpp
    ${submodules_path}/imgui/imgui_draw.cpp
    ${submodules_path}/imgui/imgui_demo.cpp
)
#add_subdirectory(raw src)

add_executable(raw ${OTHER_SOURCES} ${PROJECT_SOURCES} )
link_directories(
    ${PROJECT_SOURCE_DIR}/lib
)

target_link_libraries(raw
    PUBLIC
        glfw
        fastgltf
        glm
        GameNetworkingSockets
        Vulkan::Vulkan
        vk-bootstrap::vk-bootstrap
)

target_include_directories(raw
    PUBLIC
        "${VULKAN_SDK}/Include"
        "${submodules_path}/gns/include"
        "${submodules_path}/stb"
        "${submodules_path}/imgui"
)

