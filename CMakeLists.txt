cmake_minimum_required(VERSION 3.22)
project(raw LANGUAGES CXX)


include(ExternalProject)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(Vulkan REQUIRED)
#find_package(Vulkan COMPONENTS glslc REQUIRED)


set(OPENSSL_USE_STATIC_LIBS ON)
find_package(OpenSSL REQUIRED)

#find_package(Protobuf REQUIRED)

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



add_definitions(-DSTEAMNETWORKINGSOCKETS_STATIC_LINK=1)
add_definitions(-DABSL_OPTION_USE_STD_ORDERING=0)


#ExternalProject_Add(abslEX
#    SOURCE_DIR ${submodules_path}/absl
#    CMAKE_ARGS
#        ${GLOBAL_DEFAULT_ARGS}
#        -G Ninja
#        -DBUILD_TESTING=OFF
#        -DCMAKE_BUILD_TYPE=Release
#        -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/cmake_more_cache/absl
#        -DCMAKE_CXX_STANDARD=20
#        -DCMAKE_CXX_STANDARD_REQUIRED=ON
#        -DCMAKE_CXX_FLAGS='-static'
#        -B ${PROJECT_SOURCE_DIR}/cmake_more_cache/absl_build
#    BUILD_COMMAND ninja
#    INSTALL_COMMAND ninja install
#)


ExternalProject_Add(protoEX
    SOURCE_DIR ${submodules_path}/protobuf
    CMAKE_ARGS
        ${GLOBAL_DEFAULT_ARGS}
        -G Ninja
        -DCMAKE_BUILD_TYPE=Release
        -Dprotobuf_BUILD_TESTS=OFF
        -Dprotobuf_BUILD_SHARED_LIBS=OFF
        -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto
        -DCMAKE_CXX_STANDARD=20
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
        -DABSL_OPTION_USE_STD_ORDERING=0
        -DCMAKE_CXX_FLAGS='-static'
        -B ${PROJECT_SOURCE_DIR}/cmake_more_cache/proto_build
    BUILD_COMMAND ninja
    INSTALL_COMMAND ninja install
)



ExternalProject_Add(gnsEX
    SOURCE_DIR ${submodules_path}/gns
    CMAKE_ARGS
        ${GLOBAL_DEFAULT_ARGS}
        -G Ninja
        -DCMAKE_BUILD_TYPE=Release
        -DProtobuf_USE_STATIC_LIBS=ON
        -Dprotobuf_BUILD_TESTS=OFF
        -Dprotobuf_BUILD_SHARED_LIBS=OFF
        -DCMAKE_CXX_STANDARD=20
        -DCMAKE_CXX_STANDARD_REQUIRED=ON
        -DOPENSSL_USE_STATIC_LIBS=ON
        -Dprotobuf_BUILD_TESTS=OFF
        -Dprotobuf_BUILD_SHARED_LIBS=OFF
        -DProtobuf_LIBRARIES=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/lib/cmake/protobuf
        -DProtobuf_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/include
        -DBUILD_SHARED_LIB=OFF
        -DBUILD_STATIC_LIB=ON
        -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/cmake_more_cache/gns
        -DCMAKE_PREFIX_PATH=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/bin
        -Dprotobuf_DIR=${PROJECT_SOURCE_DIR}/cmake_more_cache/proto
        -DCMAKE_CXX_FLAGS='-static\ -I${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/include\ -L${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/lib\ -lprotobuf\ -lprotoc'
        -B "${PROJECT_SOURCE_DIR}/cmake_more_cache/gns_build"
    BUILD_COMMAND ninja
    INSTALL_COMMAND ninja install
    DEPENDS protoEX
)



add_subdirectory(${submodules_path}/simdjson)
add_subdirectory(${submodules_path}/fastgltf)
add_subdirectory(${submodules_path}/glfw)
add_subdirectory(${submodules_path}/glm)
add_subdirectory(${submodules_path}/vk_boot)
add_subdirectory(${submodules_path}/vma)


file(GLOB PROJECT_SOURCES "src/*.cpp" "src/*/*.cpp")
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


#set_target_properties(raw PROPERTIES CMAKE_CXX_FLAGS '-static\ -l${PROJECT_SOURCE_DIR}/cmake_more_cache/gns/lib/GameNetworkingSockets_s')


file(GLOB shaders ${PROJECT_BINARY_DIR}/shaders/*)
list(LENGTH shaders shader_len)

#if(shader_len EQUAL 0)
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
  add_dependencies(raw shaders)
#endif()




set(resources_bin_dir "${PROJECT_BINARY_DIR}/resources")
add_custom_command(
    OUTPUT ${resources_bin_dir}
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/resources" "${PROJECT_BINARY_DIR}/resources"
    DEPENDS "${PROJECT_SOURCE_DIR}/resources"
)
add_custom_target(resources_bin_deps DEPENDS ${PROJECT_BINARY_DIR}/resources)
add_dependencies(raw resources_bin_deps)

target_link_directories(raw PRIVATE
${PROJECT_SOURCE_DIR}/cmake_more_cache/gns/lib
${PROJECT_SOURCE_DIR}/cmake_more_cache/proto/lib

)



#add_dependencies(protoEX abslEX)
add_dependencies(gnsEX protoEX)
add_dependencies(raw gnsEX)

#add_library(absl::strings ALIAS absl_strings)

target_link_libraries(raw
    PRIVATE
        glfw
        fastgltf
        glm
        Vulkan::Vulkan
        vk-bootstrap::vk-bootstrap
        utf8_range
        GameNetworkingSockets_s
        protobuf
        absl_strings              
        absl_str_format_internal    
        absl_strings_internal         
        absl_log_initialize           
        absl_log_entry                
        absl_log_flags                
        absl_log_severity             
        absl_log_internal_check_op    
        absl_log_internal_conditions  
        absl_log_internal_message     
        absl_log_internal_nullguard   
        absl_log_internal_proto       
        absl_log_internal_format      
        absl_log_internal_globals     
        absl_log_internal_log_sink_set
        absl_log_sink                 
        absl_raw_logging_internal     
        absl_log_globals              
        #utf8_validity                 
        absl_cord                     
        absl_cordz_info               
        absl_cordz_handle             
        absl_cordz_functions          
        absl_cord_internal            
        absl_crc_cord_state           
        absl_crc32c                   
        absl_crc_internal             
        absl_exponential_biased       
        absl_synchronization          
        absl_time                     
        absl_time_zone                
        absl_int128                   
        absl_examine_stack            
        absl_stacktrace               
        absl_symbolize                
        absl_demangle_internal        
        absl_debugging_internal       
        absl_malloc_internal          
        absl_throw_delegate           
        absl_strerror                 
        absl_raw_hash_set             
        absl_hash                     
        absl_city                     
        absl_low_level_hash           
        absl_base                     
        absl_spinlock_wait
        absl_status
        #abls_synchronization_internal

)

target_include_directories(raw
    PUBLIC
        "${PROJECT_SOURCE_DIR}/cmake_more_cache/gns/include/GameNetworkingSockets"
        #"${submodules_path}/gns/include"
        "${submodules_path}/stb"
        "${submodules_path}/imgui"
        "${submodules_path}/vma/include"
        "${PROJECT_SOURCE_DIR}/src"
)



set_property(TARGET raw APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${PROJECT_BINARY_DIR}/resources)
set_property(TARGET raw APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${PROJECT_BINARY_DIR}/shaders)


