cmake_minimum_required(VERSION 3.22)
project(raw LANGUAGES CXX)



set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)



find_package(Vulkan 1.3 REQUIRED)



add_subdirectory(${PROJECT_SOURCE_DIR}/protobuf)
#if(NOT IS_DIRECTORY ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto)
#execute_process(COMMAND mkdir ${PROJECT_SOURCE_DIR}/cmake_more_cache)
#execute_process(COMMAND mkdir ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto_build)
#execute_process(COMMAND mkdir ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto)
#execute_process(COMMAND mkdir ${PROJECT_SOURCE_DIR}/cmake_more_cache/gns)
#execute_process(COMMAND cd ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto_build)
#execute_process(COMMAND "cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_BUILD_SHARED_LIBS=ON -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto ${PROJECT_SOURCE_DIR}/protobuf")
#execute_process(COMMAND ninja)
#execute_process(COMMAND "ninja install")
#execute_process(COMMAND cd ${PROJECT_SOURCE_DIR}/cmake_more_cache/gns_build)
#execute_process(COMMAND cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ${PROJECT_SOURCE_DIR}/gns)
#execute_process(COMMAND ninja)
#endif()
#libprotobuf_Add(raw
#    SOURCE_DIR ${PROJECT_SOURCE_DIR}/protobuf/cmake
#    CMAKE_ARGS
#        #${GLOBAL_DEFAULT_ARGS}
#        -G Ninja
#        -DCMAKE_BUILD_TYPE=Release
#        -Dprotobuf_BUILD_TESTS=OFF
#        -Dprotobuf_BUILD_SHARED_LIBS=ON
#        -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto
#    BUILD_COMMAND ninja
#)


add_subdirectory(${PROJECT_SOURCE_DIR}/fastgltf)
add_subdirectory(${PROJECT_SOURCE_DIR}/glfw)
add_subdirectory(${PROJECT_SOURCE_DIR}/glm)
add_subdirectory(${PROJECT_SOURCE_DIR}/gns)



set(PROJECT_SOURCES
    src/main.cpp
)

add_executable(raw ${PROJECT_SOURCES})


set_target_properties(raw
    PROPERTIES
        WIN32_EXECUTABLE TRUE
)

target_link_libraries(raw
    PUBLIC
        glfw
        fastgltf
        glm
        #protobuf
        GameNetworkingSockets
        vulkan
)

target_include_directories(raw
    PUBLIC
    ${PROJECT_SOURCE_DIR}/fastgltf/inculde/fastgltf
    ${PROJECT_SOURCE_DIR}/glm/glm
)
