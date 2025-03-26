cmake_minimum_required(VERSION 3.31)
project(rview LANGUAGES CXX)


include(ExternalProject)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Vulkan REQUIRED)

set(submodules_path
        ${PROJECT_SOURCE_DIR}/sub
)

if(UNIX)
set(SHELL_CMD bash)
set(SEP ":")
set(STATIC_LIB_EXT a)
else()
#set(SHELL_CMD pwsh)  #powershell 7
set(SHELL_CMD powershell) #default for windows 
set(EXT ".exe")
set(SEP ";")
set(STATIC_LIB_EXT lib)
endif()





set(FASTGLTF_COMPILE_AS_CPP20 ON CACHE BOOL "set" FORCE)


set(BUILD_SHARED_LIBS OFF CACHE BOOL "set" FORCE)




add_subdirectory(${submodules_path}/simdjson)
add_subdirectory(${submodules_path}/fastgltf)
add_subdirectory(${submodules_path}/sdl)
add_subdirectory(${submodules_path}/sdlimg)
add_subdirectory(${submodules_path}/glm)
add_subdirectory(${submodules_path}/vk_boot)
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

add_executable(rview ${OTHER_SOURCES} ${PROJECT_SOURCES} ${PROJECT_HEADERS})



if(UNIX)
set_target_properties(rview PROPERTIES CMAKE_CXX_FLAGS '-static')
else()
set_target_properties(rview PROPERTIES CMAKE_CXX_FLAGS '-static\ -mwindows')
endif()

file(GLOB shaders ${PROJECT_BINARY_DIR}/shaders/*)
list(LENGTH shaders shader_len)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    )
  foreach(GLSL ${GLSL_SOURCE_FILES})
    file(TO_CMAKE_PATH "${GLSL}" GLSL_NATIVE)
    cmake_path(GET GLSL_NATIVE FILENAME SPIRV_NAME)
      set(SPIRV "${PROJECT_BINARY_DIR}/shaders/${SPIRV_NAME}.spv")
      add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/shaders"
        COMMAND ${SHELL_CMD} -c "$ENV{VULKAN_SDK}/bin/glslc --target-env=vulkan1.3 --target-spv=spv1.6 -std=460 -c ${GLSL_NATIVE} -o ${SPIRV}"
        DEPENDS ${GLSL_NATIVE}
      )
      list(APPEND SPIRV_BINARY_FILES ${SPIRV})
  endforeach()
    set_source_files_properties(${SPIRV_BINARY_FILES} PROPERTIES GENERATED TRUE)
  add_custom_target(shaders ALL DEPENDS ${SPIRV_BINARY_FILES})
  add_dependencies(rview shaders)




set(resources_bin_dir "${PROJECT_BINARY_DIR}/resources")
add_custom_command(
    OUTPUT ${resources_bin_dir}
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/resources" "${PROJECT_BINARY_DIR}/resources"
    DEPENDS "${PROJECT_SOURCE_DIR}/resources"
)
add_custom_target(resources_bin_deps DEPENDS ${PROJECT_BINARY_DIR}/resources)
add_dependencies(rview resources_bin_deps)


target_link_libraries(rview
    PRIVATE
        SDL3-static
        SDL3_image-static
        fastgltf
        glm
        Vulkan::Vulkan
        vk-bootstrap::vk-bootstrap
)
if(UNIX)
target_link_libraries(rview
    PRIVATE
        c
        pthread
)
else()
target_link_libraries(rview
    PRIVATE
        #pthread
        #ws2_32                               #windows
        DbgHelp                               #windows
        winmm                                 #windows
        iphlpapi                              #windows
)
endif()

target_include_directories(rview
    PUBLIC
        "${submodules_path}/stb"
        "${submodules_path}/imgui"
        "${submodules_path}/vma/include"
        "${submodules_path}/sdl/include"
        "${PROJECT_SOURCE_DIR}/include"
)



set_property(TARGET rview APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${PROJECT_BINARY_DIR}/resources)
set_property(TARGET rview APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${PROJECT_BINARY_DIR}/shaders)


