cmake_minimum_required(VERSION 3.22)
project(raw LANGUAGES CXX)

#include(ExternalProject)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Vulkan REQUIRED)
#find_package(Vulkan COMPONENTS glslc REQUIRED)
#find_package(libprotobuf)
#find_package(GameNetworkingSockets)

set(submodules_path
        ${PROJECT_SOURCE_DIR}/sub
)

if(UNIX)
set(SHELL_CMD bash)
set(STATIC_LIB_EXT a)
else()
#set(SHELL_CMD pwsh)  #powershell 7
set(SHELL_CMD powershell) #default for windows 
set(STATIC_LIB_EXT lib)
endif()




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

add_executable(raw ${OTHER_SOURCES} ${PROJECT_SOURCES})

file(GLOB shaders ${PROJECT_BINARY_DIR}/shaders/*)
list(LENGTH shaders shader_len)
#message(WARNING ${shader_len})

#if(shader_len EQUAL 0)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    )
  set(SHADER_COMMAND)
  foreach(GLSL ${GLSL_SOURCE_FILES})
    file(TO_CMAKE_PATH "${GLSL}" GLSL_NATIVE)
    cmake_path(GET GLSL_NATIVE FILENAME SPIRV_NAME)
      set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${SPIRV_NAME}.spv")
      add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders"
        COMMAND ${SHELL_CMD} -c '$ENV{VULKAN_SDK}/bin/glslc --target-env=vulkan1.3 --target-spv=spv1.6 -std=460 -c ${GLSL_NATIVE} -o ${SPIRV}'
        DEPENDS ${GLSL_NATIVE}
      )
      list(APPEND SPIRV_BINARY_FILES ${SPIRV})
  endforeach()
  add_custom_target(shaders_dep DEPENDS ${SPIRV_BINARY_FILES})
  add_dependencies(raw shaders_dep)
#endif()


#set(resources_dir "${PROJECT_SOURCE_DIR}/resources")
#add_custom_command(
#    OUTPUT ${resources_dir}
#    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/resources" "${PROJECT_BINARY_DIR}/resources"
#)
#add_custom_target(resources_deps DEPENDS ${PROJECT_SOURCE_DIR}/resources)
#add_dependencies(raw resources_deps)


set(resources_bin_dir "${PROJECT_BINARY_DIR}/resources")
add_custom_command(
    OUTPUT ${resources_bin_dir}
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/resources" "${PROJECT_BINARY_DIR}/resources"
    DEPENDS "${PROJECT_SOURCE_DIR}/resources"
)
add_custom_target(resources_bin_deps DEPENDS ${PROJECT_BINARY_DIR}/resources)
add_dependencies(raw resources_bin_deps)

target_link_directories(raw PUBLIC ${PROJECT_SOURCE_DIR}/lib)

target_link_libraries(raw
    PUBLIC
        glfw
        fastgltf
        glm
        Vulkan::Vulkan
        vk-bootstrap::vk-bootstrap
        GameNetworkingSockets
)

target_include_directories(raw
    PUBLIC
        #"${VULKAN_SDK}/Include"
        "${submodules_path}/gns/include"
        "${submodules_path}/stb"
        "${submodules_path}/imgui"
)
